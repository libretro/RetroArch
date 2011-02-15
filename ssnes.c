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


#include <stdbool.h>
#include <libsnes.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include "driver.h"
#include "file.h"
#include "hqflt/filters.h"
#include "general.h"
#include "dynamic.h"
#include "record/ffemu.h"
#include "rewind.h"
#include "movie.h"
#include "netplay.h"
#include <assert.h>
#ifdef HAVE_SRC
#include <samplerate.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif


#ifdef __APPLE__
#include "SDL.h" 
// OSX seems to really need -lSDLmain, 
// so we include SDL.h here so it can hack our main.
// I had issues including this in Win32 for some reason. :)
#endif

struct global g_extern = {
   .video_active = true,
   .audio_active = true,
   .game_type = SSNES_CART_NORMAL,
};

// To avoid continous switching if we hold the button down, we require that the button must go from pressed, unpressed back to pressed to be able to toggle between then.
static void set_fast_forward_button(bool new_button_state)
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
         g_extern.audio_data.chunk_size = g_extern.audio_data.nonblock_chunk_size;
      else
         g_extern.audio_data.chunk_size = g_extern.audio_data.block_chunk_size;
   }
   old_button_state = new_button_state;
}

#ifdef HAVE_FILTER
static inline void process_frame (uint16_t * restrict out, const uint16_t * restrict in, unsigned width, unsigned height)
{
   int pitch = 1024;
   if (height == 448 || height == 478)
      pitch = 512;

   for (int y = 0; y < height; y++)
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

   const char *msg = msg_queue_pull(g_extern.msg_queue);

#ifdef HAVE_FILTER
   uint16_t output_filter[width * height * 4 * 4];
   uint16_t output[width * height];

   if (g_settings.video.filter != FILTER_NONE)
      process_frame(output, data, width, height);


   switch (g_settings.video.filter)
   {
      case FILTER_HQ2X:
         ProcessHQ2x(output, output_filter);
         if (!driver.video->frame(driver.video_data, output_filter, width << 1, height << 1, width << 2, msg))
            g_extern.video_active = false;
         break;
      case FILTER_HQ4X:
         ProcessHQ4x(output, output_filter);
         if (!driver.video->frame(driver.video_data, output_filter, width << 2, height << 2, width << 3, msg))
            g_extern.video_active = false;
         break;
      case FILTER_GRAYSCALE:
         grayscale_filter(output, width, height);
         if (!driver.video->frame(driver.video_data, output, width, height, width << 1, msg))
            g_extern.video_active = false;
         break;
      case FILTER_BLEED:
         bleed_filter(output, width, height);
         if (!driver.video->frame(driver.video_data, output, width, height, width << 1, msg))
            g_extern.video_active = false;
         break;
      case FILTER_NTSC:
         ntsc_filter(output_filter, output, width, height);
         if (!driver.video->frame(driver.video_data, output_filter, SNES_NTSC_OUT_WIDTH(width), height, SNES_NTSC_OUT_WIDTH(width) << 1, msg))
            g_extern.video_active = false;
         break;
      default:
         if (!driver.video->frame(driver.video_data, data, width, height, (height == 448 || height == 478) ? 1024 : 2048, msg))
            g_extern.video_active = false;
   }
#else
   if (!driver.video->frame(driver.video_data, data, width, height, (height == 448 || height == 478) ? 1024 : 2048, msg))
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

   g_extern.audio_data.data[g_extern.audio_data.data_ptr++] = (float)(int16_t)left/0x8000; 
   g_extern.audio_data.data[g_extern.audio_data.data_ptr++] = (float)(int16_t)right/0x8000;

   if (g_extern.audio_data.data_ptr >= g_extern.audio_data.chunk_size)
   {

      if (g_extern.frame_is_reverse) // Disable fucked up audio when rewinding...
         memset(g_extern.audio_data.data, 0, g_extern.audio_data.chunk_size * sizeof(float));

#ifdef HAVE_SRC
      SRC_DATA src_data = {
#else
      struct hermite_data src_data = {
#endif
         .data_in = g_extern.audio_data.data,
         .data_out = g_extern.audio_data.outsamples,
         .input_frames = g_extern.audio_data.chunk_size / 2,
         .output_frames = g_extern.audio_data.chunk_size * 8,
         .end_of_input = 0,
         .src_ratio = (double)g_settings.audio.out_rate / (double)g_settings.audio.in_rate,
      };

#ifdef HAVE_SRC
      src_process(g_extern.audio_data.source, &src_data);
#else
      hermite_process(g_extern.audio_data.source, &src_data);
#endif
      if (g_extern.audio_data.use_float)
      {
         if (driver.audio->write(driver.audio_data, g_extern.audio_data.outsamples, src_data.output_frames_gen * sizeof(float) * 2) < 0)
         {
            fprintf(stderr, "SSNES [ERROR]: Audio backend failed to write. Will continue without sound.\n");
            g_extern.audio_active = false;
         }
      }
      else
      {
         for (unsigned i = 0; i < src_data.output_frames_gen * 2; i++)
         {
            int32_t val = g_extern.audio_data.outsamples[i] * 0x8000;
            g_extern.audio_data.conv_outsamples[i] = (val > 0x7FFF) ? 0x7FFF : (val < -0x8000 ? -0x8000 : (int16_t)val);
         }

         if (driver.audio->write(driver.audio_data, g_extern.audio_data.conv_outsamples, src_data.output_frames_gen * sizeof(int16_t) * 2) < 0)
         {
            fprintf(stderr, "SSNES [ERROR]: Audio backend failed to write. Will continue without sound.\n");
            g_extern.audio_active = false;
         }
      }

      g_extern.audio_data.data_ptr = 0;
   }
}

static void input_poll(void)
{
   driver.input->poll(driver.input_data);
}

static int16_t input_state(bool port, unsigned device, unsigned index, unsigned id)
{
   if (g_extern.bsv_movie && g_extern.bsv_movie_playback)
   {
      int16_t ret;
      if (bsv_movie_get_input(g_extern.bsv_movie, &ret))
         return ret;
      else
      {
         g_extern.bsv_movie_end = true;
         return 0;
      }
   }

   const struct snes_keybind *binds[MAX_PLAYERS];
   for (int i = 0; i < MAX_PLAYERS; i++)
      binds[i] = g_settings.input.binds[i];

   int16_t res = driver.input->input_state(driver.input_data, binds, port, device, index, id);
   if (g_extern.bsv_movie && !g_extern.bsv_movie_playback)
      bsv_movie_set_input(g_extern.bsv_movie, res);

   return res;
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

#ifdef _WIN32
#define PACKAGE_VERSION "0.3-beta"
#endif

#include "config.features.h"

#define _PSUPP(var, name, desc) printf("\t%s:\n\t\t%s: %s\n", name, desc, _##var##_supp ? "yes" : "no")
static void print_features(void)
{
   puts("");
   puts("Features:");
   _PSUPP(sdl, "SDL", "SDL drivers");
   _PSUPP(alsa, "ALSA", "audio driver");
   _PSUPP(oss, "OSS", "audio driver");
   _PSUPP(jack, "Jack", "audio driver");
   _PSUPP(rsound, "RSound", "audio driver");
   _PSUPP(roar, "RoarAudio", "audio driver");
   _PSUPP(pulse, "PulseAudio", "audio driver");
   _PSUPP(xaudio, "XAudio2", "audio driver");
   _PSUPP(al, "OpenAL", "audio driver");
   _PSUPP(filter, "Filter", "CPU based video filters");
   _PSUPP(cg, "Cg", "Cg pixel shaders");
   _PSUPP(xml, "XML", "bSNES XML pixel shaders");
   _PSUPP(dynamic, "Dynamic", "Dynamic run-time loading of libsnes library");
   _PSUPP(ffmpeg, "FFmpeg", "On-the-fly recording of gameplay with libavcodec");
   _PSUPP(src, "SRC", "libsamplerate audio resampling");
   _PSUPP(configfile, "Config file", "Configuration file support");
   _PSUPP(freetype, "FreeType", "TTF font rendering with FreeType");
}
#undef _PSUPP

static void print_help(void)
{
   puts("===================================================================");
   puts("ssnes: Simple Super Nintendo Emulator (libsnes) -- v" PACKAGE_VERSION " --");
   puts("===================================================================");
   puts("Usage: ssnes [rom file] [-h/--help | -c/--config | -v/--verbose | -4/--multitap | -j/--justifier | -J/--justifiers | -S/--savestate | -m/--mouse | -g/--gameboy | -b/--bsx | -B/--bsxslot | --sufamiA | --sufamiB | -p/--scope | -s/--save" FFMPEG_HELP_QUARK "]");
   puts("\t-h/--help: Show this help message");
   puts("\t-s/--save: Path for save file (*.srm). Required when rom is input from stdin");
   puts("\t-S/--savestate: Path to use for save states. If not selected, *.state will be assumed.");
#ifdef HAVE_CONFIGFILE
   puts("\t-c/--config: Path for config file." SSNES_DEFAULT_CONF_PATH_STR);
#endif
   puts("\t-g/--gameboy: Path to Gameboy ROM. Load SuperGameBoy as the regular rom.");
   puts("\t-b/--bsx: Path to BSX rom. Load BSX BIOS as the regular rom.");
   puts("\t-B/--bsxslot: Path to BSX slotted rom. Load BSX BIOS as the regular rom.");
   puts("\t--sufamiA: Path to A slot of Sufami Turbo. Load Sufami base cart as regular rom.");
   puts("\t--sufamiB: Path to B slot of Sufami Turbo.");
   puts("\t-m/--mouse: Connect a virtual mouse into designated port of the SNES (1 or 2)."); 
   puts("\t\tThis argument can be specified several times to connect more mice.");
   puts("\t-p/--scope: Connect a virtual SuperScope into port 2 of the SNES.");
   puts("\t-j/--justifier: Connect a virtual Konami Justifier into port 2 of the SNES.");
   puts("\t-J/--justifiers: Daisy chain two virtual Konami Justifiers into port 2 of the SNES.");
   puts("\t-4/--multitap: Connect a multitap to port 2 of the SNES.");
   puts("\t-P/--bsvplay: Playback a BSV movie file.");
   puts("\t-H/--host: Host netplay as player 1.");
   puts("\t-C/--connect: Connect to netplay as player 2.");
   puts("\t-F/--frames: Sync frames when using netplay.");

#ifdef HAVE_FFMPEG
   puts("\t-r/--record: Path to record video file. Settings for video/audio codecs are found in config file.");
#endif
   puts("\t-v/--verbose: Verbose logging");

   print_features();
}

static void set_basename(const char *path)
{
   char tmp[strlen(path) + 1];
   strcpy(tmp, path);
   char *dst = strrchr(tmp, '.');
   if (dst)
      *dst = '\0';
   strncpy(g_extern.basename, tmp, sizeof(g_extern.basename) - 1);
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
      { "gameboy", 1, NULL, 'g' },
#ifdef HAVE_CONFIGFILE
      { "config", 0, NULL, 'c' },
#endif
      { "mouse", 1, NULL, 'm' },
      { "scope", 0, NULL, 'p' },
      { "savestate", 1, NULL, 'S' },
      { "bsx", 1, NULL, 'b' },
      { "bsxslot", 1, NULL, 'B' },
      { "justifier", 0, NULL, 'j' },
      { "justifiers", 0, NULL, 'J' },
      { "multitap", 0, NULL, '4' },
      { "sufamiA", 1, NULL, 'Y' },
      { "sufamiB", 1, NULL, 'Z' },
      { "bsvplay", 1, NULL, 'P' },
      { "host", 0, NULL, 'H' },
      { "connect", 1, NULL, 'C' },
      { "frames", 1, NULL, 'F' },
      { NULL, 0, NULL, 0 }
   };

   int option_index = 0;

#ifdef HAVE_FFMPEG
#define FFMPEG_RECORD_ARG "r:"
#else
#define FFMPEG_RECORD_ARG
#endif

#ifdef HAVE_CONFIGFILE
#define CONFIG_FILE_ARG "c:"
#else
#define CONFIG_FILE_ARG
#endif

   char optstring[] = "hs:vS:m:p4jJg:b:B:Y:Z:P:HC:F:" FFMPEG_RECORD_ARG CONFIG_FILE_ARG;
   for(;;)
   {
      int c = getopt_long(argc, argv, optstring, opts, &option_index);
      int port;

      if (c == -1)
         break;

      switch (c)
      {
         case 'h':
            print_help();
            exit(0);

         case '4':
            g_extern.has_multitap = true;
            break;

         case 'j':
            g_extern.has_justifier = true;
            break;

         case 'J':
            g_extern.has_justifiers = true;
            break;

         case 's':
            strncpy(g_extern.savefile_name_srm, optarg, sizeof(g_extern.savefile_name_srm) - 1);
            g_extern.has_set_save_path = true;
            break;

         case 'g':
            strncpy(g_extern.gb_rom_path, optarg, sizeof(g_extern.gb_rom_path) - 1);
            g_extern.game_type = SSNES_CART_SGB;
            break;

         case 'b':
            strncpy(g_extern.bsx_rom_path, optarg, sizeof(g_extern.bsx_rom_path) - 1);
            g_extern.game_type = SSNES_CART_BSX;
            break;

         case 'B':
            strncpy(g_extern.bsx_rom_path, optarg, sizeof(g_extern.bsx_rom_path) - 1);
            g_extern.game_type = SSNES_CART_BSX_SLOTTED;
            break;

         case 'Y':
            strncpy(g_extern.sufami_rom_path[0], optarg, sizeof(g_extern.sufami_rom_path[0]) - 1);
            g_extern.game_type = SSNES_CART_SUFAMI;
            break;

         case 'Z':
            strncpy(g_extern.sufami_rom_path[1], optarg, sizeof(g_extern.sufami_rom_path[1]) - 1);
            g_extern.game_type = SSNES_CART_SUFAMI;
            break;

         case 'S':
            strncpy(g_extern.savestate_name, optarg, sizeof(g_extern.savestate_name) - 1);
            g_extern.has_set_state_path = true;
            break;

         case 'v':
            g_extern.verbose = true;
            break;

         case 'm':
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > 2)
            {
               SSNES_ERR("Connect mouse to port 1 or 2.\n");
               print_help();
               exit(1);
            }
            g_extern.has_mouse[port - 1] = true;
            break;

         case 'p':
            g_extern.has_scope[1] = true;
            break;

#ifdef HAVE_CONFIGFILE
         case 'c':
            strncpy(g_extern.config_path, optarg, sizeof(g_extern.config_path) - 1);
            break;
#endif

#ifdef HAVE_FFMPEG
         case 'r':
            strncpy(g_extern.record_path, optarg, sizeof(g_extern.record_path) - 1);
            g_extern.recording = true;
            break;
#endif

         case 'P':
            strncpy(g_extern.bsv_movie_path, optarg, sizeof(g_extern.bsv_movie_path) - 1);
            g_extern.bsv_movie_playback = true;
            break;

         case 'H':
            g_extern.netplay_enable = true;
            break;

         case 'C':
            g_extern.netplay_enable = true;
            strncpy(g_extern.netplay_server, optarg, sizeof(g_extern.netplay_server) - 1);
            break;

         case 'F':
            g_extern.netplay_sync_frames = strtol(optarg, NULL, 0);
            if (g_extern.netplay_sync_frames < 32)
               g_extern.netplay_sync_frames = 32;
            break;

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
      set_basename(argv[optind]);
     
      SSNES_LOG("Opening file: \"%s\"\n", argv[optind]);
      g_extern.rom_file = fopen(argv[optind], "rb");
      if (g_extern.rom_file == NULL)
      {
         SSNES_ERR("Could not open file: \"%s\"\n", argv[optind]);
         exit(1);
      }
      // strl* would be nice :D
      if (!g_extern.has_set_save_path)
         fill_pathname(g_extern.savefile_name_srm, g_extern.basename, ".srm");
      if (!g_extern.has_set_state_path)
         fill_pathname(g_extern.savestate_name, g_extern.basename, ".state");
   }
   else if (strlen(g_extern.savefile_name_srm) == 0)
   {
      SSNES_ERR("Need savefile path argument (--save) when reading rom from stdin.\n");
      print_help();
      exit(1);
   }
   else if (strlen(g_extern.savestate_name) == 0)
   {
      SSNES_ERR("Need savestate path argument (--savefile) when reading rom from stdin.\n");
      print_help();
      exit(1);
   }
}

// TODO: Add rest of the controllers.
static void init_controllers(void)
{
   if (g_extern.has_justifier)
   {
      SSNES_LOG("Connecting Justifier to port 2.\n");
      psnes_set_controller_port_device(SNES_PORT_2, SNES_DEVICE_JUSTIFIER);
   }
   else if (g_extern.has_justifiers)
   {
      SSNES_LOG("Connecting Justifiers to port 2.\n");
      psnes_set_controller_port_device(SNES_PORT_2, SNES_DEVICE_JUSTIFIERS);
   }
   else if (g_extern.has_multitap)
   {
      SSNES_LOG("Connecting multitap to port 2.\n");
      psnes_set_controller_port_device(SNES_PORT_2, SNES_DEVICE_MULTITAP);
   }
   else
   {
      for (int i = 0; i < 2; i++)
      {
         if (g_extern.has_mouse[i])
         {
            SSNES_LOG("Connecting mouse to port %d\n", i + 1);
            psnes_set_controller_port_device(i, SNES_DEVICE_MOUSE);
         }
         else if (g_extern.has_scope[i])
         {
            SSNES_LOG("Connecting scope to port %d\n", i + 1);
            psnes_set_controller_port_device(i, SNES_DEVICE_SUPER_SCOPE);
         }
      }
   }
}

static inline void load_save_files(void)
{
   switch (g_extern.game_type)
   {
      case SSNES_CART_NORMAL:
         load_ram_file(g_extern.savefile_name_srm, SNES_MEMORY_CARTRIDGE_RAM);
         load_ram_file(g_extern.savefile_name_rtc, SNES_MEMORY_CARTRIDGE_RTC);
         break;

      case SSNES_CART_SGB:
         save_ram_file(g_extern.savefile_name_srm, SNES_MEMORY_GAME_BOY_RAM);
         save_ram_file(g_extern.savefile_name_rtc, SNES_MEMORY_GAME_BOY_RTC);
         break;

      case SSNES_CART_BSX:
      case SSNES_CART_BSX_SLOTTED:
         load_ram_file(g_extern.savefile_name_srm, SNES_MEMORY_BSX_RAM);
         load_ram_file(g_extern.savefile_name_psrm, SNES_MEMORY_BSX_PRAM);
         break;

      case SSNES_CART_SUFAMI:
         load_ram_file(g_extern.savefile_name_asrm, SNES_MEMORY_SUFAMI_TURBO_A_RAM);
         load_ram_file(g_extern.savefile_name_bsrm, SNES_MEMORY_SUFAMI_TURBO_B_RAM);
         break;

      default:
         break;
   }
}

static inline void save_files(void)
{
   switch (g_extern.game_type)
   {
      case SSNES_CART_NORMAL:
         save_ram_file(g_extern.savefile_name_srm, SNES_MEMORY_CARTRIDGE_RAM);
         save_ram_file(g_extern.savefile_name_rtc, SNES_MEMORY_CARTRIDGE_RTC);
         break;

      case SSNES_CART_SGB:
         save_ram_file(g_extern.savefile_name_srm, SNES_MEMORY_GAME_BOY_RAM);
         save_ram_file(g_extern.savefile_name_rtc, SNES_MEMORY_GAME_BOY_RTC);
         break;

      case SSNES_CART_BSX:
      case SSNES_CART_BSX_SLOTTED:
         save_ram_file(g_extern.savefile_name_srm, SNES_MEMORY_BSX_RAM);
         save_ram_file(g_extern.savefile_name_psrm, SNES_MEMORY_BSX_PRAM);
         break;

      case SSNES_CART_SUFAMI:
         save_ram_file(g_extern.savefile_name_asrm, SNES_MEMORY_SUFAMI_TURBO_A_RAM);
         save_ram_file(g_extern.savefile_name_bsrm, SNES_MEMORY_SUFAMI_TURBO_B_RAM);
         break;

      default:
         break;
   }
}


#ifdef HAVE_FFMPEG
static void init_recording(void)
{
   // Hardcode these options at the moment. Should be specificed in the config file later on.
   if (g_extern.recording)
   {
      struct ffemu_rational ntsc_fps = {60000, 1000};
      struct ffemu_rational pal_fps = {50000, 1000};
      struct ffemu_params params = {
         .vcodec = FFEMU_VIDEO_H264,
         .acodec = FFEMU_AUDIO_VORBIS,
         .rescaler = FFEMU_RESCALER_POINT,
         .out_width = 512,
         .out_height = 448,
         .channels = 2,
         .samplerate = 32000,
         .filename = g_extern.record_path,
         .fps = psnes_get_region() == SNES_REGION_NTSC ? ntsc_fps : pal_fps,
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
}

static void deinit_recording(void)
{
   if (g_extern.recording)
   {
      ffemu_finalize(g_extern.rec);
      ffemu_free(g_extern.rec);
   }
}
#endif

static void init_msg_queue(void)
{
   g_extern.msg_queue = msg_queue_new(8);
   assert(g_extern.msg_queue);
}

static void deinit_msg_queue(void)
{
   if (g_extern.msg_queue)
      msg_queue_free(g_extern.msg_queue);
}

static void init_rewind(void)
{
   if (g_settings.rewind_enable)
   {
      size_t serial_size = psnes_serialize_size();
      g_extern.state_buf = malloc((serial_size + 3) & ~3); // Make sure we allocate at least 4-byte multiple.
      psnes_serialize(g_extern.state_buf, serial_size);
      SSNES_LOG("Initing rewind buffer with size: %u MB\n", (unsigned)g_settings.rewind_buffer_size / 1000000);
      g_extern.state_manager = state_manager_new((serial_size + 3) & ~3, g_settings.rewind_buffer_size, g_extern.state_buf);
      if (!g_extern.state_manager)
         SSNES_WARN("Failed to init rewind buffer. Rewinding will be disabled!\n");
   }
}

static void deinit_rewind(void)
{
   if (g_extern.state_manager)
      state_manager_free(g_extern.state_manager);
   if (g_extern.state_buf)
      free(g_extern.state_buf);
}

static void init_movie(void)
{
   if (g_extern.bsv_movie_playback)
   {
      g_extern.bsv_movie = bsv_movie_init(g_extern.bsv_movie_path, SSNES_MOVIE_PLAYBACK);
      if (!g_extern.bsv_movie)
      {
         SSNES_ERR("Failed to load movie file: \"%s\"!\n", g_extern.bsv_movie_path);
         exit(1);
      }

      msg_queue_push(g_extern.msg_queue, "Starting movie playback!", 2, 180);
      SSNES_LOG("Starting movie playback!\n");
   }
}

static void deinit_movie(void)
{
   if (g_extern.bsv_movie)
      bsv_movie_free(g_extern.bsv_movie);
}

static void init_netplay(void)
{
   if (g_extern.netplay_enable)
   {
      struct snes_callbacks cbs = {
         .frame_cb = video_frame,
         .sample_cb = audio_sample,
         .poll_cb = input_poll,
         .state_cb = input_state
      };

      if (strlen(g_extern.netplay_server) > 0)
      {
         SSNES_LOG("Connecting to netplay host...\n");
         g_extern.netplay_is_client = true;
      }
      else
         SSNES_LOG("Waiting for client...\n");

      g_extern.netplay = netplay_new(g_extern.netplay_is_client ? g_extern.netplay_server : NULL, 55435, g_extern.netplay_sync_frames, &cbs);
      if (!g_extern.netplay)
      {
         g_extern.netplay_is_client = false;
         SSNES_WARN("Failed to init netplay...\n");
      }
   }
}

static void deinit_netplay(void)
{
   if (g_extern.netplay)
      netplay_free(g_extern.netplay);
}

static void init_autosave(void)
{
   int ram_types[2] = {-1, -1};
   const char *ram_paths[2] = {NULL, NULL};

   switch (g_extern.game_type)
   {
      case SSNES_CART_BSX:
      case SSNES_CART_BSX_SLOTTED:
         ram_types[0] = SNES_MEMORY_BSX_RAM;
         ram_types[1] = SNES_MEMORY_BSX_PRAM;
         ram_paths[0] = g_extern.savefile_name_srm;
         ram_paths[1] = g_extern.savefile_name_psrm;
         break;

      case SSNES_CART_SUFAMI:
         ram_types[0] = SNES_MEMORY_SUFAMI_TURBO_A_RAM;
         ram_types[1] = SNES_MEMORY_SUFAMI_TURBO_B_RAM;
         ram_paths[0] = g_extern.savefile_name_asrm;
         ram_paths[1] = g_extern.savefile_name_bsrm;
         break;

      case SSNES_CART_SGB:
         ram_types[0] = SNES_MEMORY_GAME_BOY_RAM;
         ram_types[1] = SNES_MEMORY_GAME_BOY_RTC;
         ram_paths[0] = g_extern.savefile_name_srm;
         ram_paths[1] = g_extern.savefile_name_rtc;
         break;

      default:
         ram_types[0] = SNES_MEMORY_CARTRIDGE_RAM;
         ram_types[1] = SNES_MEMORY_CARTRIDGE_RTC;
         ram_paths[0] = g_extern.savefile_name_srm;
         ram_paths[1] = g_extern.savefile_name_rtc;
   }

   if (g_settings.autosave_interval > 0)
   {
      for (unsigned i = 0; i < sizeof(g_extern.autosave)/sizeof(g_extern.autosave[0]); i++)
      {
         if (ram_paths[i] && strlen(ram_paths[i]) > 0 && psnes_get_memory_size(ram_types[i]) > 0)
         {
            g_extern.autosave[i] = autosave_new(ram_paths[i], 
                  psnes_get_memory_data(ram_types[i]), 
                  psnes_get_memory_size(ram_types[i]), 
                  g_settings.autosave_interval);
            if (!g_extern.autosave[i])
               SSNES_WARN("Could not initialize autosave.\n");
         }
      }
   }
}

static void deinit_autosave(void)
{
   for (unsigned i = 0; i < sizeof(g_extern.autosave)/sizeof(g_extern.autosave[0]); i++)
   {
      if (g_extern.autosave[i])
         autosave_free(g_extern.autosave[i]);
   }
}

static void lock_autosave(void)
{
   for (unsigned i = 0; i < sizeof(g_extern.autosave)/sizeof(g_extern.autosave[0]); i++)
   {
      if (g_extern.autosave[i])
         autosave_lock(g_extern.autosave[i]);
   }
}

static void unlock_autosave(void)
{
   for (unsigned i = 0; i < sizeof(g_extern.autosave)/sizeof(g_extern.autosave[0]); i++)
   {
      if (g_extern.autosave[i])
         autosave_unlock(g_extern.autosave[i]);
   }
}

static void fill_pathnames(void)
{
   switch (g_extern.game_type)
   {
      case SSNES_CART_BSX:
      case SSNES_CART_BSX_SLOTTED:
         // BSX PSRM
         if (!g_extern.has_set_save_path)
            fill_pathname(g_extern.savefile_name_srm, g_extern.bsx_rom_path, ".srm");
         if (!g_extern.has_set_state_path)
            fill_pathname(g_extern.savestate_name, g_extern.bsx_rom_path, ".state");
         fill_pathname(g_extern.savefile_name_psrm, g_extern.savefile_name_srm, ".psrm");
         break;

      case SSNES_CART_SUFAMI:
         // SUFAMI ARAM
         fill_pathname(g_extern.savefile_name_asrm, g_extern.savefile_name_srm, ".asrm");
         // SUFAMI BRAM
         fill_pathname(g_extern.savefile_name_bsrm, g_extern.savefile_name_srm, ".bsrm");
         break;

      case SSNES_CART_SGB:
         if (!g_extern.has_set_save_path)
            fill_pathname(g_extern.savefile_name_srm, g_extern.gb_rom_path, ".srm");
         if (!g_extern.has_set_state_path)
            fill_pathname(g_extern.savestate_name, g_extern.gb_rom_path, ".state");
         fill_pathname(g_extern.savefile_name_rtc, g_extern.savefile_name_srm, ".rtc");
         break;

      default:
         // Infer .rtc save path from save ram path.
         fill_pathname(g_extern.savefile_name_rtc, g_extern.savefile_name_srm, ".rtc");
   }

   if (!g_extern.bsv_movie_playback)
      fill_pathname(g_extern.bsv_movie_path, g_extern.savefile_name_srm, "");
}

// Save or load state here.
static void check_savestates(void)
{
   static bool old_should_savestate = false;
   bool should_savestate = driver.input->key_pressed(driver.input_data, SSNES_SAVE_STATE_KEY);
   if (should_savestate && !old_should_savestate)
   {
      char save_path[strlen(g_extern.savestate_name) * 2];

      if (g_extern.state_slot > 0)
         snprintf(save_path, sizeof(save_path), "%s%u", g_extern.savestate_name, g_extern.state_slot);
      else
         snprintf(save_path, sizeof(save_path), "%s", g_extern.savestate_name);

      if(!save_state(save_path))
      {
         msg_queue_clear(g_extern.msg_queue);
         char msg[512];
         snprintf(msg, sizeof(msg), "Failed to save state to \"%s\"", save_path);
         msg_queue_push(g_extern.msg_queue, msg, 2, 180);
      }
      else
      {
         msg_queue_clear(g_extern.msg_queue);
         msg_queue_push(g_extern.msg_queue, "Saved state!", 1, 180);
      }
   }
   old_should_savestate = should_savestate;

   static bool old_should_loadstate = false;
   bool should_loadstate = driver.input->key_pressed(driver.input_data, SSNES_LOAD_STATE_KEY);
   if (!should_savestate && should_loadstate && !old_should_loadstate)
   {
      char load_path[strlen(g_extern.savestate_name) * 2];

      if (g_extern.state_slot > 0)
         snprintf(load_path, sizeof(load_path), "%s%u", g_extern.savestate_name, g_extern.state_slot);
      else
         snprintf(load_path, sizeof(load_path), "%s", g_extern.savestate_name);

      if(!load_state(load_path))
      {
         msg_queue_clear(g_extern.msg_queue);
         char msg[512];
         snprintf(msg, sizeof(msg), "Failed to load state from \"%s\"", load_path);
         msg_queue_push(g_extern.msg_queue, msg, 2, 180);
      }
      else
      {
         msg_queue_clear(g_extern.msg_queue);
         msg_queue_push(g_extern.msg_queue, "Loaded state!", 1, 180);
      }
   }
   old_should_loadstate = should_loadstate;
}

static void check_fullscreen(void)
{
   // If we go fullscreen we drop all drivers and reinit to be safe.
   if (driver.input->key_pressed(driver.input_data, SSNES_FULLSCREEN_TOGGLE_KEY))
   {
      g_settings.video.fullscreen = !g_settings.video.fullscreen;
      uninit_drivers();
      init_drivers();
   }
}

static void check_stateslots(void)
{
   // Save state slots
   static bool old_should_slot_increase = false;
   bool should_slot_increase = driver.input->key_pressed(driver.input_data, SSNES_STATE_SLOT_PLUS);
   if (should_slot_increase && !old_should_slot_increase)
   {
      g_extern.state_slot++;
      msg_queue_clear(g_extern.msg_queue);
      char msg[256];
      snprintf(msg, sizeof(msg), "Save state/movie slot: %u", g_extern.state_slot);
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      SSNES_LOG("%s\n", msg);
   }
   old_should_slot_increase = should_slot_increase;

   static bool old_should_slot_decrease = false;
   bool should_slot_decrease = driver.input->key_pressed(driver.input_data, SSNES_STATE_SLOT_MINUS);
   if (should_slot_decrease && !old_should_slot_decrease)
   {
      if (g_extern.state_slot > 0)
         g_extern.state_slot--;
      msg_queue_clear(g_extern.msg_queue);
      char msg[256];
      snprintf(msg, sizeof(msg), "Save state/movie slot: %u", g_extern.state_slot);
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      SSNES_LOG("%s\n", msg);
   }
   old_should_slot_decrease = should_slot_decrease;
}

static void check_input_rate(void)
{
   bool display = false;
   if (driver.input->key_pressed(driver.input_data, SSNES_AUDIO_INPUT_RATE_PLUS))
   {
      g_settings.audio.in_rate += g_settings.audio.rate_step;
      display = true;
   }
   else if (driver.input->key_pressed(driver.input_data, SSNES_AUDIO_INPUT_RATE_MINUS))
   {
      g_settings.audio.in_rate -= g_settings.audio.rate_step;
      display = true;
   }

   if (display)
   {
      char msg[256];
      snprintf(msg, sizeof(msg), "Audio input rate: %.2f Hz", g_settings.audio.in_rate);

      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, msg, 0, 180);
      SSNES_LOG("%s\n", msg);
   }
}

static void check_rewind(void)
{
   g_extern.frame_is_reverse = false;
   if (!g_extern.state_manager)
      return;

   if (driver.input->key_pressed(driver.input_data, SSNES_REWIND))
   {
      msg_queue_clear(g_extern.msg_queue);
      void *buf;
      if (state_manager_pop(g_extern.state_manager, &buf))
      {
         msg_queue_push(g_extern.msg_queue, "Rewinding!", 0, 30);
         psnes_unserialize(buf, psnes_serialize_size());
         g_extern.frame_is_reverse = true;
      }
      else
         msg_queue_push(g_extern.msg_queue, "Reached end of rewind buffer!", 0, 30);
   }
   else
   {
      static unsigned cnt = 0;
      cnt = (cnt + 1) % (g_settings.rewind_granularity ? g_settings.rewind_granularity : 1); // Avoid possible SIGFPE.
      if (cnt == 0)
      {
         psnes_serialize(g_extern.state_buf, psnes_serialize_size());
         state_manager_push(g_extern.state_manager, g_extern.state_buf);
      }
   }
}

static void check_movie_record(void)
{
   static bool old_button = false;
   bool new_button;
   if ((new_button = driver.input->key_pressed(driver.input_data, SSNES_MOVIE_RECORD_TOGGLE)) && !old_button)
   {
      if (g_extern.bsv_movie)
      {
         msg_queue_clear(g_extern.msg_queue);
         msg_queue_push(g_extern.msg_queue, "Stopping movie record!", 2, 180);
         SSNES_LOG("Stopping movie record!\n");
         bsv_movie_free(g_extern.bsv_movie);
         g_extern.bsv_movie = NULL;
      }
      else
      {
         char path[512];
         if (g_extern.state_slot > 0)
            snprintf(path, sizeof(path), "%s%d.bsv", g_extern.bsv_movie_path, g_extern.state_slot);
         else
            snprintf(path, sizeof(path), "%s.bsv", g_extern.bsv_movie_path);

         g_extern.bsv_movie = bsv_movie_init(path, SSNES_MOVIE_RECORD);
         msg_queue_clear(g_extern.msg_queue);
         msg_queue_push(g_extern.msg_queue, g_extern.bsv_movie ? "Starting movie record!" : "Failed to start movie record!", 1, 180);

         if (g_extern.bsv_movie)
            SSNES_LOG("Starting movie record!\n");
         else
            SSNES_ERR("Failed to start movie record!\n");
      }
   }

   old_button = new_button;
}

static void check_pause(void)
{
   static bool old_state = false;
   bool new_state = driver.input->key_pressed(driver.input_data, SSNES_PAUSE_TOGGLE);

   static bool old_focus = true;
   bool focus = true;

   if (g_settings.pause_nonactive)
      focus = driver.video->focus(driver.video_data);

   if (focus && new_state && !old_state)
   {
      g_extern.is_paused = !g_extern.is_paused;

      if (g_extern.is_paused)
      {
         SSNES_LOG("Paused!\n");
         if (driver.audio_data)
            driver.audio->stop(driver.audio_data);
      }
      else 
      {
         SSNES_LOG("Unpaused!\n");
         if (driver.audio_data)
         {
            if (!driver.audio->start(driver.audio_data))
            {
               SSNES_ERR("Failed to resume audio driver! Will continue without audio!\n");
               g_extern.audio_active = false;
            }
         }
      }
   }
   else if (focus && !old_focus)
   {
      SSNES_LOG("Unpaused!\n");
      g_extern.is_paused = false;
      if (driver.audio_data)
      {
         if (!driver.audio->start(driver.audio_data))
         {
               SSNES_ERR("Failed to resume audio driver! Will continue without audio!\n");
               g_extern.audio_active = false;
         }
      }
   }
   else if (!focus && old_focus)
   {
      SSNES_LOG("Paused!\n");
      g_extern.is_paused = true;
      if (driver.audio_data)
         driver.audio->stop(driver.audio_data);
   }

   old_focus = focus;
   old_state = new_state;
}

static void do_state_checks(void)
{
   if (!g_extern.netplay)
   {
      check_pause();
      if (g_extern.is_paused)
         return;

      set_fast_forward_button(driver.input->key_pressed(driver.input_data, SSNES_FAST_FORWARD_KEY));

      if (!g_extern.bsv_movie)
      {
         check_stateslots();
         check_savestates();
         check_rewind();
      }

      if (!g_extern.bsv_movie_playback)
         check_movie_record();
   }

   check_fullscreen();
   check_input_rate();
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

   fill_pathnames();

   if (!init_rom_file(g_extern.game_type))
      goto error;

   init_msg_queue();
   init_movie();

   if (!g_extern.bsv_movie)
   {
      load_save_files();
      init_rewind();
   }

   init_netplay();
   init_drivers();

   psnes_set_video_refresh(g_extern.netplay ? video_frame_net : video_frame);
   psnes_set_audio_sample(g_extern.netplay ? audio_sample_net : audio_sample);
   psnes_set_input_poll(g_extern.netplay ? input_poll_net : input_poll);
   psnes_set_input_state(g_extern.netplay ? input_state_net : input_state);
   
   init_controllers();
   
#ifdef HAVE_FFMPEG
   init_recording();
#endif

   if (!g_extern.bsv_movie_playback && !g_extern.netplay_is_client)
      init_autosave();

   // Main loop
   for(;;)
   {
      // Time to drop?
      if (driver.input->key_pressed(driver.input_data, SSNES_QUIT_KEY) ||
            !driver.video->alive(driver.video_data) || g_extern.bsv_movie_end)
         break;

      // Checks for stuff like fullscreen, save states, etc.
      do_state_checks();
      
      // Run libsnes for one frame.
      if (!g_extern.is_paused)
      {
         lock_autosave();

         if (g_extern.netplay)
            netplay_pre_frame(g_extern.netplay);

         psnes_run();

         if (g_extern.netplay)
            netplay_post_frame(g_extern.netplay);

         unlock_autosave();
      }
      else
      {
         input_poll();
#ifdef _WIN32
         Sleep(10);
#else
         struct timespec tv = {
            .tv_sec = 0,
            .tv_nsec = 10000000
         };
         nanosleep(&tv, NULL);
#endif
      }
   }

   deinit_netplay();

   if (!g_extern.bsv_movie_playback && !g_extern.netplay_is_client)
      deinit_autosave();

#ifdef HAVE_FFMPEG
   deinit_recording();
#endif

   if (!g_extern.bsv_movie_playback && !g_extern.netplay_is_client)
   {
      deinit_rewind();
      save_files();
   }

   deinit_movie();
   deinit_msg_queue();

   psnes_unload_cartridge();
   psnes_term();
   uninit_drivers();
   uninit_dlsym();

   return 0;

error:
   psnes_unload_cartridge();
   psnes_term();
   uninit_drivers();
   uninit_dlsym();

   return 1;
}


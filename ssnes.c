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
#include "driver.h"
#include "file.h"
#include "hqflt/filters.h"
#include "general.h"
#include "dynamic.h"
#include "record/ffemu.h"
#include "rewind.h"
#include <assert.h>
#ifdef HAVE_SRC
#include <samplerate.h>
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

   g_extern.audio_data.data[g_extern.audio_data.data_ptr++] = (float)(*(int16_t*)&left)/0x8000; 
   g_extern.audio_data.data[g_extern.audio_data.data_ptr++] = (float)(*(int16_t*)&right)/0x8000;

   if (g_extern.audio_data.data_ptr >= g_extern.audio_data.chunk_size)
   {

      if (g_extern.frame_is_reverse) // Disable fucked up audio when rewinding...
         memset(g_extern.audio_data.data, 0, g_extern.audio_data.chunk_size * sizeof(float));

      SRC_DATA src_data;

      src_data.data_in = g_extern.audio_data.data;
      src_data.data_out = g_extern.audio_data.outsamples;
      src_data.input_frames = g_extern.audio_data.chunk_size / 2;
      src_data.output_frames = g_extern.audio_data.chunk_size * 8;
      src_data.end_of_input = 0;
      src_data.src_ratio = (double)g_settings.audio.out_rate / (double)g_settings.audio.in_rate;

      src_process(g_extern.source, &src_data);

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
         src_float_to_short_array(g_extern.audio_data.outsamples, g_extern.audio_data.conv_outsamples, src_data.output_frames_gen * 2);

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
   const struct snes_keybind *binds[MAX_PLAYERS];
   for (int i = 0; i < MAX_PLAYERS; i++)
      binds[i] = g_settings.input.binds[i];

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

#ifdef _WIN32
#define PACKAGE_VERSION "0.2.1"
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
   puts("=============================================================");
   puts("ssnes: Simple Super Nintendo Emulator (libsnes) -- v" PACKAGE_VERSION " --");
   puts("=============================================================");
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

#ifdef HAVE_FFMPEG
   puts("\t-r/--record: Path to record video file. Settings for video/audio codecs are found in config file.");
#endif
   puts("\t-v/--verbose: Verbose logging");

   print_features();
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

   char optstring[] = "hs:vS:m:p4jJg:b:B:Y:Z:" FFMPEG_RECORD_ARG CONFIG_FILE_ARG;
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
      // strl* would be nice :D
      if (strlen(g_extern.savefile_name_srm) == 0)
      {
         strcpy(g_extern.savefile_name_srm, g_extern.basename);
         size_t len = strlen(g_extern.savefile_name_srm);
         strncat(g_extern.savefile_name_srm, ".srm", sizeof(g_extern.savefile_name_srm) - len - 1);
      }
      if (strlen(g_extern.savestate_name) == 0)
      {
         strcpy(g_extern.savestate_name, g_extern.basename);
         size_t len = strlen(g_extern.savestate_name);
         strncat(g_extern.savestate_name, ".state", sizeof(g_extern.savestate_name) - len - 1);
      }
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
      case SSNES_CART_SGB:
         load_ram_file(g_extern.savefile_name_srm, SNES_MEMORY_CARTRIDGE_RAM);
         load_ram_file(g_extern.savefile_name_rtc, SNES_MEMORY_CARTRIDGE_RTC);
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
      case SSNES_CART_SGB:
         save_ram_file(g_extern.savefile_name_srm, SNES_MEMORY_CARTRIDGE_RAM);
         save_ram_file(g_extern.savefile_name_rtc, SNES_MEMORY_CARTRIDGE_RTC);
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
      size_t serial_size = snes_serialize_size();
      g_extern.state_buf = malloc((serial_size + 3) & ~3); // Make sure we allocate at least 4-byte multiple.
      snes_serialize(g_extern.state_buf, serial_size);
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

static void fill_pathnames(void)
{
   switch (g_extern.game_type)
   {
      case SSNES_CART_BSX:
      case SSNES_CART_BSX_SLOTTED:
         // BSX PSRM
         fill_pathname(g_extern.savefile_name_psrm, g_extern.savefile_name_srm, ".psrm");
         break;

      case SSNES_CART_SUFAMI:
         // SUFAMI ARAM
         fill_pathname(g_extern.savefile_name_asrm, g_extern.savefile_name_srm, ".asrm");
         // SUFAMI BRAM
         fill_pathname(g_extern.savefile_name_bsrm, g_extern.savefile_name_srm, ".bsrm");
         break;

      default:
         // Infer .rtc save path from save ram path.
         fill_pathname(g_extern.savefile_name_rtc, g_extern.savefile_name_srm, ".rtc");
   }
}

// Save or load state here.
static void check_savestates(void)
{
   static bool old_should_savestate = false;
   bool should_savestate = driver.input->key_pressed(driver.input_data, SSNES_SAVE_STATE_KEY);
   if (should_savestate && !old_should_savestate)
   {
      char save_path[strlen(g_extern.savestate_name) * 2];
      snprintf(save_path, sizeof(save_path), g_extern.state_slot > 0 ? "%s%u" : "%s", g_extern.savestate_name, g_extern.state_slot);
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
      snprintf(load_path, sizeof(load_path), g_extern.state_slot ? "%s%u" : "%s", g_extern.savestate_name, g_extern.state_slot);

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
      snprintf(msg, sizeof(msg), "Save state slot: %u", g_extern.state_slot);
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
      snprintf(msg, sizeof(msg), "Save state slot: %u", g_extern.state_slot);
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
         snes_unserialize(buf, snes_serialize_size());
         g_extern.frame_is_reverse = true;
      }
      else
      {
         msg_queue_push(g_extern.msg_queue, "Reached end of rewind buffer!", 0, 30);
      }
   }
   else
   {
      snes_serialize(g_extern.state_buf, snes_serialize_size());
      state_manager_push(g_extern.state_manager, g_extern.state_buf);
   }
}

static void do_state_checks(void)
{
   set_fast_forward_button(driver.input->key_pressed(driver.input_data, SSNES_FAST_FORWARD_KEY));

   check_stateslots();
   check_savestates();
   check_fullscreen();
   check_input_rate();
   check_rewind();
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

   init_drivers();

   psnes_set_video_refresh(video_frame);
   psnes_set_audio_sample(audio_sample);
   psnes_set_input_poll(input_poll);
   psnes_set_input_state(input_state);
   
   init_controllers();

   load_save_files();

#ifdef HAVE_FFMPEG
   init_recording();
#endif

   init_msg_queue();
   init_rewind();

   // Main loop
   for(;;)
   {
      // Time to drop?
      if (driver.input->key_pressed(driver.input_data, SSNES_QUIT_KEY) ||
            !driver.video->alive(driver.video_data))
         break;

      // Checks for stuff like fullscreen, save states, etc.
      do_state_checks();
      
      // Run libsnes for one frame.
      psnes_run();
   }

   deinit_rewind();
   deinit_msg_queue();
#ifdef HAVE_FFMPEG
   deinit_recording();
#endif

   save_files();

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


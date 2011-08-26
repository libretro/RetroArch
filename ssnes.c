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
#include "general.h"
#include "dynamic.h"
#include "record/ffemu.h"
#include "rewind.h"
#include "movie.h"
#include "netplay.h"
#include "strl.h"
#include "screenshot.h"
#include "cheats.h"
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
// We want to use -mconsole in Win32, so we need main().
#endif

// To avoid continous switching if we hold the button down, we require that the button must go from pressed, unpressed back to pressed to be able to toggle between then.
static void set_fast_forward_button(bool new_button_state, bool new_hold_button_state)
{
   bool update_sync = false;
   static bool old_button_state = false;
   static bool old_hold_button_state = false;
   static bool syncing_state = false;

   if (new_button_state && !old_button_state)
   {
      syncing_state = !syncing_state;
      update_sync = true;
   }
   else if (old_hold_button_state != new_hold_button_state)
   {
      syncing_state = new_hold_button_state;
      update_sync = true;
   }

   if (update_sync)
   {
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
   old_hold_button_state = new_hold_button_state;
}

static void take_screenshot(const uint16_t *frame, unsigned width, unsigned height)
{
   if (!(*g_settings.screenshot_directory))
      return;

   bool ret = screenshot_dump(g_settings.screenshot_directory, frame, 
         width, height, (height == 448 || height == 478) ? 1024 : 2048);

   const char *msg = NULL;
   if (ret)
   {
      SSNES_LOG("Taking screenshot!\n");
      msg = "Taking screenshot!";
   }
   else
   {
      SSNES_WARN("Failed to take screenshot ...\n");
      msg = "Failed to take screenshot!";
   }

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
}

// libsnes: 0.065
// Format received is 16-bit 0RRRRRGGGGGBBBBB
static void video_frame(const uint16_t *data, unsigned width, unsigned height)
{
   if (!g_extern.video_active)
      return;

   if (g_settings.video.crop_overscan)
   {
      if (height == 239)
      {
         data += 7 * 1024; // Skip 7 top scanlines.
         height = 224;
      }
      else if (height == 478)
      {
         data += 15 * 512; // Skip 15 top scanlines.
         height = 448;
      }
   }

   if (g_extern.do_screenshot)
      take_screenshot(data, width, height);

   // Slightly messy code,
   // but we really need to do processing before blocking on VSync for best possible scheduling.
#ifdef HAVE_FFMPEG
   if (g_extern.recording && (!g_extern.filter.active || !g_settings.video.post_filter_record))
   {
      struct ffemu_video_data ffemu_data = {
         .data = data,
         .pitch = height == 448 || height == 478 ? 1024 : 2048,
         .width = width,
         .height = height,
      };
      ffemu_push_video(g_extern.rec, &ffemu_data);
   }
#endif

   const char *msg = msg_queue_pull(g_extern.msg_queue);

#ifdef HAVE_DYLIB
   if (g_extern.filter.active)
   {
      unsigned owidth = width;
      unsigned oheight = height;
      g_extern.filter.psize(&owidth, &oheight);
      g_extern.filter.prender(g_extern.filter.colormap, g_extern.filter.buffer, 
            g_extern.filter.pitch, data, (height == 448 || height == 478) ? 1024 : 2048, width, height);

#ifdef HAVE_FFMPEG
      if (g_extern.recording && g_settings.video.post_filter_record)
      {
         struct ffemu_video_data ffemu_data = {
            .data = g_extern.filter.buffer,
            .pitch = g_extern.filter.pitch,
            .width = owidth,
            .height = oheight,
         };
         ffemu_push_video(g_extern.rec, &ffemu_data);
      }
#endif

      if (!driver.video->frame(driver.video_data, g_extern.filter.buffer, owidth, oheight, g_extern.filter.pitch, msg))
         g_extern.video_active = false;
   }
   else if (!driver.video->frame(driver.video_data, data, width, height, (height == 448 || height == 478) ? 1024 : 2048, msg))
      g_extern.video_active = false;
#else
   if (!driver.video->frame(driver.video_data, data, width, height, (height == 448 || height == 478) ? 1024 : 2048, msg))
      g_extern.video_active = false;
#endif
}

static void audio_sample(uint16_t left, uint16_t right)
{
   if (!g_extern.audio_active)
      return;

   const float *output_data = NULL;
   unsigned output_frames = 0;

#ifdef HAVE_FFMPEG
   g_extern.audio_data.conv_outsamples[g_extern.audio_data.data_ptr] = left;
#endif
   g_extern.audio_data.data[g_extern.audio_data.data_ptr++] = (float)(int16_t)left/0x8000; 
#ifdef HAVE_FFMPEG
   g_extern.audio_data.conv_outsamples[g_extern.audio_data.data_ptr] = right;
#endif
   g_extern.audio_data.data[g_extern.audio_data.data_ptr++] = (float)(int16_t)right/0x8000;

   if (g_extern.audio_data.data_ptr < g_extern.audio_data.chunk_size)
      return;

#ifdef HAVE_FFMPEG
   if (g_extern.recording)
   {
      struct ffemu_audio_data ffemu_data = {
         .data = g_extern.audio_data.conv_outsamples,
         .frames = g_extern.audio_data.data_ptr / 2
      };
      ffemu_push_audio(g_extern.rec, &ffemu_data);
   }
#endif

   ssnes_dsp_input_t dsp_input = {
      .samples = g_extern.audio_data.data,
      .frames = g_extern.audio_data.data_ptr / 2
   };

   ssnes_dsp_output_t dsp_output = {
      .should_resample = SSNES_TRUE
   };

   if (g_extern.audio_data.dsp_plugin)
      g_extern.audio_data.dsp_plugin->process(g_extern.audio_data.dsp_handle, &dsp_output, &dsp_input);

   if (g_extern.frame_is_reverse) // Disable fucked up audio when rewinding...
      memset(g_extern.audio_data.data, 0, g_extern.audio_data.chunk_size * sizeof(float));

#ifdef HAVE_SRC
   SRC_DATA src_data = {
#else
   struct hermite_data src_data = {
#endif
      .data_in = dsp_output.samples ? dsp_output.samples : g_extern.audio_data.data,
      .data_out = g_extern.audio_data.outsamples,
      .input_frames = dsp_output.samples ? dsp_output.frames : (g_extern.audio_data.data_ptr / 2),
      .output_frames = g_extern.audio_data.chunk_size * 8,
      .end_of_input = 0,
      .src_ratio = (double)g_settings.audio.out_rate / (double)g_settings.audio.in_rate,
   };

   if (dsp_output.should_resample)
   {
#ifdef HAVE_SRC
      src_process(g_extern.audio_data.source, &src_data);
#else
      hermite_process(g_extern.audio_data.source, &src_data);
#endif

      output_data = g_extern.audio_data.outsamples;
      output_frames = src_data.output_frames_gen;
   }
   else
   {
      output_data = dsp_output.samples;
      output_frames = dsp_output.frames;
   }


   if (g_extern.audio_data.use_float)
   {
      if (driver.audio->write(driver.audio_data, output_data, output_frames * sizeof(float) * 2) < 0)
      {
         fprintf(stderr, "SSNES [ERROR]: Audio backend failed to write. Will continue without sound.\n");
         g_extern.audio_active = false;
      }
   }
   else
   {
      for (unsigned i = 0; i < output_frames * 2; i++)
      {
         int32_t val = output_data[i] * 0x8000;
         g_extern.audio_data.conv_outsamples[i] = (val > 0x7FFF) ? 0x7FFF : (val < -0x8000 ? -0x8000 : (int16_t)val);
      }

      if (driver.audio->write(driver.audio_data, g_extern.audio_data.conv_outsamples, output_frames * sizeof(int16_t) * 2) < 0)
      {
         fprintf(stderr, "SSNES [ERROR]: Audio backend failed to write. Will continue without sound.\n");
         g_extern.audio_active = false;
      }
   }

   g_extern.audio_data.data_ptr = 0;
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

   static const struct snes_keybind *binds[MAX_PLAYERS] = {
      g_settings.input.binds[0],
      g_settings.input.binds[1],
      g_settings.input.binds[2],
      g_settings.input.binds[3],
      g_settings.input.binds[4]
   };

   int16_t res = driver.input->input_state(driver.input_data, binds, port, device, index, id);
   if (g_extern.bsv_movie && !g_extern.bsv_movie_playback)
      bsv_movie_set_input(g_extern.bsv_movie, res);

   return res;
}

#ifdef _WIN32
#define SSNES_DEFAULT_CONF_PATH_STR "\n\t\tDefaults to ssnes.cfg in same directory as ssnes.exe."
#elif defined(__APPLE__)
#define SSNES_DEFAULT_CONF_PATH_STR " Defaults to $HOME/.ssnes.cfg."
#else
#define SSNES_DEFAULT_CONF_PATH_STR " Defaults to $XDG_CONFIG_HOME/ssnes/ssnes.cfg,\n\t\tor $HOME/.ssnes.cfg, if $XDG_CONFIG_HOME is not defined."
#endif

#ifdef _WIN32
#define PACKAGE_VERSION "0.7"
#endif

#include "config.features.h"

#define _PSUPP(var, name, desc) printf("\t%s:\n\t\t%s: %s\n", name, desc, _##var##_supp ? "yes" : "no")
static void print_features(void)
{
   puts("");
   puts("Features:");
   _PSUPP(sdl, "SDL", "SDL drivers");
   _PSUPP(xvideo, "XVideo", "XVideo output");
   _PSUPP(alsa, "ALSA", "audio driver");
   _PSUPP(oss, "OSS", "audio driver");
   _PSUPP(jack, "Jack", "audio driver");
   _PSUPP(rsound, "RSound", "audio driver");
   _PSUPP(roar, "RoarAudio", "audio driver");
   _PSUPP(pulse, "PulseAudio", "audio driver");
   _PSUPP(xaudio, "XAudio2", "audio driver");
   _PSUPP(al, "OpenAL", "audio driver");
   _PSUPP(dylib, "External", "External filter and driver support");
   _PSUPP(cg, "Cg", "Cg pixel shaders");
   _PSUPP(xml, "XML", "bSNES XML pixel shaders");
   _PSUPP(sdl_image, "SDL_image", "SDL_image image loading");
   _PSUPP(fbo, "FBO", "OpenGL render-to-texture (multi-pass shaders)");
   _PSUPP(dynamic, "Dynamic", "Dynamic run-time loading of libsnes library");
   _PSUPP(ffmpeg, "FFmpeg", "On-the-fly recording of gameplay with libavcodec");
   _PSUPP(src, "SRC", "libsamplerate audio resampling");
   _PSUPP(configfile, "Config file", "Configuration file support");
   _PSUPP(freetype, "FreeType", "TTF font rendering with FreeType");
   _PSUPP(netplay, "Netplay", "Peer-to-peer netplay");
   _PSUPP(python, "Python", "Script support in shaders");
}
#undef _PSUPP

static void print_help(void)
{
   puts("===================================================================");
   puts("ssnes: Simple Super Nintendo Emulator (libsnes) -- v" PACKAGE_VERSION " --");
   puts("===================================================================");
   puts("Usage: ssnes [rom file] [options...]");
   puts("\t-h/--help: Show this help message.");
   puts("\t-s/--save: Path for save file (*.srm). Required when rom is input from stdin.");
   puts("\t-f/--fullscreen: Start SSNES in fullscreen regardless of config settings.");
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
   puts("\t-N/--nodevice: Disconnects the controller device connected to the emulated SNES (1 or 2).");
   puts("\t\tThis argument can be specified several times to connect more mice.");
   puts("\t-p/--scope: Connect a virtual SuperScope into port 2 of the SNES.");
   puts("\t-j/--justifier: Connect a virtual Konami Justifier into port 2 of the SNES.");
   puts("\t-J/--justifiers: Daisy chain two virtual Konami Justifiers into port 2 of the SNES.");
   puts("\t-4/--multitap: Connect a multitap to port 2 of the SNES.");
   puts("\t-P/--bsvplay: Playback a BSV movie file.");
#ifdef HAVE_NETPLAY
   puts("\t-H/--host: Host netplay as player 1.");
   puts("\t-C/--connect: Connect to netplay as player 2.");
   puts("\t--port: Port used to netplay. Default is 55435.");
   puts("\t-F/--frames: Sync frames when using netplay.");
#endif

#ifdef HAVE_FFMPEG
   puts("\t-r/--record: Path to record video file.\n\t\tSettings for video/audio codecs are found in config file.");
#endif
   puts("\t-v/--verbose: Verbose logging.");
   puts("\t-U/--ups: Specifies path for UPS patch that will be applied to ROM.");
   puts("\t--bps: Specifies path for BPS patch that will be applied to ROM.");
   puts("\t-X/--xml: Specifies path to XML memory map.");
   puts("\t-D/--detach: Detach SSNES from the running console. Not relevant for all platforms.\n");

   print_features();
}

static void set_basename(const char *path)
{
   strlcpy(g_extern.basename, path, sizeof(g_extern.basename));
   char *dst = strrchr(g_extern.basename, '.');
   if (dst)
      *dst = '\0';
}

static void set_paths(const char *path)
{
   set_basename(path);

   SSNES_LOG("Opening file: \"%s\"\n", path);
   g_extern.rom_file = fopen(path, "rb");
   if (g_extern.rom_file == NULL)
   {
      SSNES_ERR("Could not open file: \"%s\"\n", path);
      exit(1);
   }

   if (!g_extern.has_set_save_path)
      fill_pathname_noext(g_extern.savefile_name_srm, g_extern.basename, ".srm", sizeof(g_extern.savefile_name_srm));
   if (!g_extern.has_set_state_path)
      fill_pathname_noext(g_extern.savestate_name, g_extern.basename, ".state", sizeof(g_extern.savestate_name));

   if (path_is_directory(g_extern.savefile_name_srm))
   {
      fill_pathname_dir(g_extern.savefile_name_srm, g_extern.basename, ".srm", sizeof(g_extern.savefile_name_srm));
      SSNES_LOG("Redirecting save file to \"%s\".\n", g_extern.savefile_name_srm);
   }
   if (path_is_directory(g_extern.savestate_name))
   {
      fill_pathname_dir(g_extern.savestate_name, g_extern.basename, ".state", sizeof(g_extern.savestate_name));
      SSNES_LOG("Redirecting save state to \"%s\".\n", g_extern.savestate_name);
   }
   if (*g_extern.config_path && path_is_directory(g_extern.config_path))
   {
      fill_pathname_dir(g_extern.config_path, g_extern.basename, ".cfg", sizeof(g_extern.config_path));
      SSNES_LOG("Redirecting config file to \"%s\".\n", g_extern.config_path);
      if (!path_file_exists(g_extern.config_path))
      {
         *g_extern.config_path = '\0';
         SSNES_LOG("Did not find config file. Using system default.\n");
      }
   }
}

static void verify_stdin_paths(void)
{
   if (strlen(g_extern.savefile_name_srm) == 0)
   {
      SSNES_ERR("Need savefile path argument (--save) when reading rom from stdin.\n");
      print_help();
      exit(1);
   }
   else if (strlen(g_extern.savestate_name) == 0)
   {
      SSNES_ERR("Need savestate path argument (--savestate) when reading rom from stdin.\n");
      print_help();
      exit(1);
   }

   if (path_is_directory(g_extern.savefile_name_srm))
   {
      SSNES_ERR("Cannot specify directory for path argument (--save) when reading from stdin.\n");
      print_help();
      exit(1);
   }
   else if (path_is_directory(g_extern.savestate_name))
   {
      SSNES_ERR("Cannot specify directory for path argument (--savestate) when reading from stdin.\n");
      print_help();
      exit(1);
   }
   else if (path_is_directory(g_extern.config_path))
   {
      SSNES_ERR("Cannot specify directory for config file (--config) when reading from stdin.\n");
      print_help();
      exit(1);
   }
}

static void parse_input(int argc, char *argv[])
{
   if (argc < 2)
   {
      print_help();
      exit(1);
   }

   int val = 0;

   struct option opts[] = {
      { "help", 0, NULL, 'h' },
      { "save", 1, NULL, 's' },
      { "fullscreen", 0, NULL, 'f' },
#ifdef HAVE_FFMPEG
      { "record", 1, NULL, 'r' },
#endif
      { "verbose", 0, NULL, 'v' },
      { "gameboy", 1, NULL, 'g' },
#ifdef HAVE_CONFIGFILE
      { "config", 0, NULL, 'c' },
#endif
      { "mouse", 1, NULL, 'm' },
      { "nodevice", 1, NULL, 'N' },
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
      { "port", 1, &val, 'p' },
      { "ups", 1, NULL, 'U' },
      { "bps", 1, &val, 'B' },
      { "xml", 1, NULL, 'X' },
      { "detach", 0, NULL, 'D' },
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

   char optstring[] = "hs:fvS:m:p4jJg:b:B:Y:Z:P:HC:F:U:DN:X:" FFMPEG_RECORD_ARG CONFIG_FILE_ARG;
   for(;;)
   {
      val = 0;
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
            strlcpy(g_extern.savefile_name_srm, optarg, sizeof(g_extern.savefile_name_srm));
            g_extern.has_set_save_path = true;
            break;

         case 'f':
            g_extern.force_fullscreen = true;
            break;

         case 'g':
            strlcpy(g_extern.gb_rom_path, optarg, sizeof(g_extern.gb_rom_path));
            g_extern.game_type = SSNES_CART_SGB;
            break;

         case 'b':
            strlcpy(g_extern.bsx_rom_path, optarg, sizeof(g_extern.bsx_rom_path));
            g_extern.game_type = SSNES_CART_BSX;
            break;

         case 'B':
            strlcpy(g_extern.bsx_rom_path, optarg, sizeof(g_extern.bsx_rom_path));
            g_extern.game_type = SSNES_CART_BSX_SLOTTED;
            break;

         case 'Y':
            strlcpy(g_extern.sufami_rom_path[0], optarg, sizeof(g_extern.sufami_rom_path[0]));
            g_extern.game_type = SSNES_CART_SUFAMI;
            break;

         case 'Z':
            strlcpy(g_extern.sufami_rom_path[1], optarg, sizeof(g_extern.sufami_rom_path[1]));
            g_extern.game_type = SSNES_CART_SUFAMI;
            break;

         case 'S':
            strlcpy(g_extern.savestate_name, optarg, sizeof(g_extern.savestate_name));
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

         case 'N':
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > 2)
            {
               SSNES_ERR("Disconnected device from port 1 or 2.\n");
               print_help();
               exit(1);
            }
            g_extern.disconnect_device[port - 1] = true;
            break;

         case 'p':
            g_extern.has_scope[1] = true;
            break;

#ifdef HAVE_CONFIGFILE
         case 'c':
            strlcpy(g_extern.config_path, optarg, sizeof(g_extern.config_path));
            break;
#endif

#ifdef HAVE_FFMPEG
         case 'r':
            strlcpy(g_extern.record_path, optarg, sizeof(g_extern.record_path));
            g_extern.recording = true;
            break;
#endif

         case 'P':
            strlcpy(g_extern.bsv_movie_path, optarg, sizeof(g_extern.bsv_movie_path));
            g_extern.bsv_movie_playback = true;
            break;

         case 'H':
            g_extern.netplay_enable = true;
            break;

         case 'C':
            g_extern.netplay_enable = true;
            strlcpy(g_extern.netplay_server, optarg, sizeof(g_extern.netplay_server));
            break;

         case 'F':
            g_extern.netplay_sync_frames = strtol(optarg, NULL, 0);
            if (g_extern.netplay_sync_frames > 32)
               g_extern.netplay_sync_frames = 32;
            break;

         case 'U':
            strlcpy(g_extern.ups_name, optarg, sizeof(g_extern.ups_name));
            g_extern.ups_pref = true;
            break;

         case 'X':
            strlcpy(g_extern.xml_name, optarg, sizeof(g_extern.xml_name));
            break;

         case 'D':
#ifdef _WIN32
            FreeConsole();
#endif
            break;

         case 0:
            switch (val)
            {
               case 'p':
                  g_extern.netplay_port = strtoul(optarg, NULL, 0);
                  break;
               case 'B':
                  strlcpy(g_extern.bps_name, optarg, sizeof(g_extern.bps_name));
                  g_extern.bps_pref = true;
                  break;
               default:
                  break;
            }
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
      set_paths(argv[optind]);
   else
      verify_stdin_paths();
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
      SSNES_LOG("Connecting Multitap to port 2.\n");
      psnes_set_controller_port_device(SNES_PORT_2, SNES_DEVICE_MULTITAP);
   }
   else
   {
      for (unsigned i = 0; i < 2; i++)
      {
         if (g_extern.disconnect_device[i])
         {
            SSNES_LOG("Disconnecting device from port %u.\n", i + 1);
            psnes_set_controller_port_device(i, SNES_DEVICE_NONE);
         }
         else if (g_extern.has_mouse[i])
         {
            SSNES_LOG("Connecting mouse to port %u.\n", i + 1);
            psnes_set_controller_port_device(i, SNES_DEVICE_MOUSE);
         }
         else if (g_extern.has_scope[i])
         {
            SSNES_LOG("Connecting scope to port %u.\n", i + 1);
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
         SSNES_LOG("Saving regular SRAM.\n");
         save_ram_file(g_extern.savefile_name_srm, SNES_MEMORY_CARTRIDGE_RAM);
         save_ram_file(g_extern.savefile_name_rtc, SNES_MEMORY_CARTRIDGE_RTC);
         break;

      case SSNES_CART_SGB:
         SSNES_LOG("Saving Gameboy SRAM.\n");
         save_ram_file(g_extern.savefile_name_srm, SNES_MEMORY_GAME_BOY_RAM);
         save_ram_file(g_extern.savefile_name_rtc, SNES_MEMORY_GAME_BOY_RTC);
         break;

      case SSNES_CART_BSX:
      case SSNES_CART_BSX_SLOTTED:
         SSNES_LOG("Saving BSX (P)RAM.\n");
         save_ram_file(g_extern.savefile_name_srm, SNES_MEMORY_BSX_RAM);
         save_ram_file(g_extern.savefile_name_psrm, SNES_MEMORY_BSX_PRAM);
         break;

      case SSNES_CART_SUFAMI:
         SSNES_LOG("Saving Sufami turbo A/B RAM.\n");
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
   if (g_extern.recording)
   {
      // Not perfectly SNES accurate, but this can be adjusted later during processing
      // and playback losslessly, and we please the containers more by
      // using "sane" values.
      struct ffemu_rational ntsc_fps = {60000, 1000};
      struct ffemu_rational pal_fps = {50000, 1000};
      struct ffemu_params params = {
         .out_width = 256,
         .out_height = 224,
         .fb_width = 1024,
         .fb_height = 512,
         .channels = 2,
         .samplerate = 32000,
         .filename = g_extern.record_path,
         .fps = psnes_get_region() == SNES_REGION_NTSC ? ntsc_fps : pal_fps,
         .aspect_ratio = 4.0 / 3,
         .rgb32 = false,
      };

      if (g_settings.video.hires_record)
      {
         params.out_width = 512;
         params.out_height = 448;
      }

      if (g_settings.video.post_filter_record && g_extern.filter.active)
      {
         g_extern.filter.psize(&params.out_width, &params.out_height);
         params.rgb32 = true;

         unsigned max_width = 512;
         unsigned max_height = 512;
         g_extern.filter.psize(&max_width, &max_height);
         params.fb_width = g_extern.filter.pitch >> 2;
         params.fb_height = next_pow2(max_height);
      }

      SSNES_LOG("Recording with FFmpeg to %s @ %ux%u. (FB size: %ux%u 32-bit: %s)\n", g_extern.record_path, params.out_width, params.out_height, params.fb_width, params.fb_height, params.rgb32 ? "yes" : "no");
      g_extern.rec = ffemu_new(&params);
      if (!g_extern.rec)
      {
         SSNES_ERR("Failed to start FFmpeg recording.\n");
         g_extern.recording = false;
      }
      else
         g_settings.video.crop_overscan = true;
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

#ifdef HAVE_XML
static void init_cheats(void)
{
   if (*g_settings.cheat_database)
      g_extern.cheat = cheat_manager_new(g_settings.cheat_database);
}

static void deinit_cheats(void)
{
   if (g_extern.cheat)
      cheat_manager_free(g_extern.cheat);
}
#endif

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
      g_settings.rewind_granularity = 1;
      SSNES_LOG("Starting movie playback!\n");
   }
}

static void deinit_movie(void)
{
   if (g_extern.bsv_movie)
      bsv_movie_free(g_extern.bsv_movie);
}

#define SSNES_DEFAULT_PORT 55435

#ifdef HAVE_NETPLAY
static void init_netplay(void)
{
   if (g_extern.netplay_enable)
   {
      struct snes_callbacks cbs = {
         .frame_cb = video_frame,
         .sample_cb = audio_sample,
         .state_cb = input_state
      };

      if (strlen(g_extern.netplay_server) > 0)
      {
         SSNES_LOG("Connecting to netplay host...\n");
         g_extern.netplay_is_client = true;
      }
      else
         SSNES_LOG("Waiting for client...\n");

      g_extern.netplay = netplay_new(g_extern.netplay_is_client ? g_extern.netplay_server : NULL, g_extern.netplay_port ? g_extern.netplay_port : SSNES_DEFAULT_PORT, g_extern.netplay_sync_frames, &cbs);
      if (!g_extern.netplay)
      {
         g_extern.netplay_is_client = false;
         SSNES_WARN("Failed to init netplay...\n");
      }
   }
}
#endif

#ifdef HAVE_NETPLAY
static void deinit_netplay(void)
{
   if (g_extern.netplay)
      netplay_free(g_extern.netplay);
}
#endif

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

static void fill_pathnames(void)
{
   switch (g_extern.game_type)
   {
      case SSNES_CART_BSX:
      case SSNES_CART_BSX_SLOTTED:
         // BSX PSRM
         if (!g_extern.has_set_save_path)
            fill_pathname(g_extern.savefile_name_srm, g_extern.bsx_rom_path, ".srm", sizeof(g_extern.savefile_name_srm));
         if (!g_extern.has_set_state_path)
            fill_pathname(g_extern.savestate_name, g_extern.bsx_rom_path, ".state", sizeof(g_extern.savestate_name));
         fill_pathname(g_extern.savefile_name_psrm, g_extern.savefile_name_srm, ".psrm", sizeof(g_extern.savefile_name_psrm));
         break;

      case SSNES_CART_SUFAMI:
         // SUFAMI ARAM
         fill_pathname(g_extern.savefile_name_asrm, g_extern.savefile_name_srm, ".asrm", sizeof(g_extern.savefile_name_asrm));
         // SUFAMI BRAM
         fill_pathname(g_extern.savefile_name_bsrm, g_extern.savefile_name_srm, ".bsrm", sizeof(g_extern.savefile_name_bsrm));
         break;

      case SSNES_CART_SGB:
         if (!g_extern.has_set_save_path)
            fill_pathname(g_extern.savefile_name_srm, g_extern.gb_rom_path, ".srm", sizeof(g_extern.savefile_name_srm));
         if (!g_extern.has_set_state_path)
            fill_pathname(g_extern.savestate_name, g_extern.gb_rom_path, ".state", sizeof(g_extern.savestate_name));
         fill_pathname(g_extern.savefile_name_rtc, g_extern.savefile_name_srm, ".rtc", sizeof(g_extern.savefile_name_rtc));
         break;

      default:
         // Infer .rtc save path from save ram path.
         fill_pathname(g_extern.savefile_name_rtc, g_extern.savefile_name_srm, ".rtc", sizeof(g_extern.savefile_name_rtc));
   }

   if (!g_extern.bsv_movie_playback)
      fill_pathname(g_extern.bsv_movie_path, g_extern.savefile_name_srm, "", sizeof(g_extern.bsv_movie_path));

   if (*g_extern.basename)
   {
      if (!(*g_extern.ups_name))
         fill_pathname_noext(g_extern.ups_name, g_extern.basename, ".ups", sizeof(g_extern.ups_name));

      if (!(*g_extern.bps_name))
         fill_pathname_noext(g_extern.bps_name, g_extern.basename, ".bps", sizeof(g_extern.bps_name));

      if (!(*g_extern.xml_name))
         fill_pathname_noext(g_extern.xml_name, g_extern.basename, ".xml", sizeof(g_extern.xml_name));
   }
}

// Save or load state here.
static void check_savestates(void)
{
   static bool old_should_savestate = false;
   bool should_savestate = driver.input->key_pressed(driver.input_data, SSNES_SAVE_STATE_KEY);
   if (should_savestate && !old_should_savestate)
   {
      char save_path[MAXPATHLEN];

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
      char load_path[MAXPATHLEN];

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
   static bool was_pressed = false;
   bool pressed;
   // If we go fullscreen we drop all drivers and reinit to be safe.
   if ((pressed = driver.input->key_pressed(driver.input_data, SSNES_FULLSCREEN_TOGGLE_KEY)) && !was_pressed)
   {
      g_settings.video.fullscreen = !g_settings.video.fullscreen;
      uninit_drivers();
      init_drivers();
   }

   was_pressed = pressed;
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
   static bool first = true;
   if (first)
   {
      first = false;
      return;
   }

   if (!g_extern.state_manager)
      return;

   if (driver.input->key_pressed(driver.input_data, SSNES_REWIND))
   {
      msg_queue_clear(g_extern.msg_queue);
      void *buf;
      if (state_manager_pop(g_extern.state_manager, &buf))
      {
         g_extern.frame_is_reverse = true;
         msg_queue_push(g_extern.msg_queue, "Rewinding!", 0, 30);
         psnes_unserialize(buf, psnes_serialize_size());

         if (g_extern.bsv_movie)
         {
            bsv_movie_frame_rewind(g_extern.bsv_movie);
         }
      }
      else
         msg_queue_push(g_extern.msg_queue, "Reached end of rewind buffer!", 0, 30);
   }
   else
   {
      static unsigned cnt = 0;
      cnt = (cnt + 1) % (g_settings.rewind_granularity ? g_settings.rewind_granularity : 1); // Avoid possible SIGFPE.
      if (cnt == 0 || g_extern.bsv_movie)
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
         g_settings.rewind_granularity = 1;
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

static void check_reset(void)
{
   if (driver.input->key_pressed(driver.input_data, SSNES_RESET))
   {
      SSNES_LOG("Resetting game!\n");
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, "Reset!", 1, 120);
      psnes_reset();
      init_controllers(); // bSNES since v073r01 resets controllers to JOYPAD after a reset, so just enforce it here.
   }
}

#ifdef HAVE_XML
static void check_shader_dir(void)
{
   static bool old_pressed_next = false;
   static bool old_pressed_prev = false;

   if (!g_extern.shader_dir.elems || !driver.video->xml_shader)
      return;

   bool should_apply = false;
   bool pressed_next = driver.input->key_pressed(driver.input_data, SSNES_SHADER_NEXT);
   bool pressed_prev = driver.input->key_pressed(driver.input_data, SSNES_SHADER_PREV);
   if (pressed_next && !old_pressed_next)
   {
      should_apply = true;
      g_extern.shader_dir.ptr = (g_extern.shader_dir.ptr + 1) % g_extern.shader_dir.size;
   }
   else if (pressed_prev && !old_pressed_prev)
   {
      should_apply = true;
      if (g_extern.shader_dir.ptr == 0)
         g_extern.shader_dir.ptr = g_extern.shader_dir.size - 1;
      else
         g_extern.shader_dir.ptr--;
   }

   if (should_apply)
   {
      const char *shader = g_extern.shader_dir.elems[g_extern.shader_dir.ptr];

      strlcpy(g_settings.video.bsnes_shader_path, shader, sizeof(g_settings.video.bsnes_shader_path));
      g_settings.video.shader_type = SSNES_SHADER_BSNES;

      msg_queue_clear(g_extern.msg_queue);
      char msg[512];
      snprintf(msg, sizeof(msg), "XML shader #%u: \"%s\"", (unsigned)g_extern.shader_dir.ptr, shader);
      msg_queue_push(g_extern.msg_queue, msg, 1, 120);
      SSNES_LOG("Applying shader \"%s\"\n", shader);

      if (!driver.video->xml_shader(driver.video_data, shader))
         SSNES_WARN("Failed to apply shader!\n");
   }

   old_pressed_next = pressed_next;
   old_pressed_prev = pressed_prev;

}

static void check_cheats(void)
{
   if (!g_extern.cheat)
      return;

   static bool old_pressed_prev = false;
   static bool old_pressed_next = false;
   static bool old_pressed_toggle = false;

   bool pressed_next = driver.input->key_pressed(driver.input_data, SSNES_CHEAT_INDEX_PLUS);
   bool pressed_prev = driver.input->key_pressed(driver.input_data, SSNES_CHEAT_INDEX_MINUS);
   bool pressed_toggle = driver.input->key_pressed(driver.input_data, SSNES_CHEAT_TOGGLE);

   if (pressed_next && !old_pressed_next)
      cheat_manager_index_next(g_extern.cheat);
   else if (pressed_prev && !old_pressed_prev)
      cheat_manager_index_prev(g_extern.cheat);
   else if (pressed_toggle && !old_pressed_toggle)
      cheat_manager_toggle(g_extern.cheat);

   old_pressed_prev = pressed_prev;
   old_pressed_next = pressed_next;
   old_pressed_toggle = pressed_toggle;
}
#endif

static void check_screenshot(void)
{
   static bool old_pressed = false;
   bool pressed = driver.input->key_pressed(driver.input_data, SSNES_SCREENSHOT);
   g_extern.do_screenshot = pressed && !old_pressed;
   old_pressed = pressed;
}

#ifdef HAVE_DYLIB
static void check_dsp_config(void)
{
   if (!g_extern.audio_data.dsp_plugin || !g_extern.audio_data.dsp_plugin->config)
      return;

   static bool old_pressed = false;
   bool pressed = driver.input->key_pressed(driver.input_data, SSNES_DSP_CONFIG);
   if (pressed && !old_pressed)
      g_extern.audio_data.dsp_plugin->config(g_extern.audio_data.dsp_handle);

   old_pressed = pressed;
}
#endif

static void do_state_checks(void)
{
   if (!g_extern.netplay)
   {
      check_reset();
#ifdef HAVE_XML
      check_cheats();
#endif
      check_pause();
      if (g_extern.is_paused)
         return;

      set_fast_forward_button(
            driver.input->key_pressed(driver.input_data, SSNES_FAST_FORWARD_KEY),
            driver.input->key_pressed(driver.input_data, SSNES_FAST_FORWARD_HOLD_KEY));

      if (!g_extern.bsv_movie)
      {
         check_stateslots();
         check_savestates();
      }
      check_rewind();

      if (!g_extern.bsv_movie_playback)
         check_movie_record();

#ifdef HAVE_XML
      check_shader_dir();
#endif

#ifdef HAVE_DYLIB
      check_dsp_config();
#endif
   }

   check_fullscreen();

#ifdef HAVE_DYLIB
   // DSP plugin doesn't use variable input rate.
   if (!g_extern.audio_data.dsp_plugin)
#endif
      check_input_rate();

   check_screenshot();
}

static void fill_title_buf(void)
{
   if (psnes_library_id)
      snprintf(g_extern.title_buf, sizeof(g_extern.title_buf), "SSNES : %s", psnes_library_id());
   else
      snprintf(g_extern.title_buf, sizeof(g_extern.title_buf), "SSNES");
}

static void init_state(void)
{
   // Make absolutely sure our big global structs are in-fact zeroed out.
   memset(&g_extern, 0, sizeof(g_extern));
   memset(&g_settings, 0, sizeof(g_settings));
   g_extern.video_active = true;
   g_extern.audio_active = true;
   g_extern.game_type = SSNES_CART_NORMAL;
}

int main(int argc, char *argv[])
{
   init_state();

   parse_input(argc, argv);
   parse_config();
   init_dlsym();
   fill_title_buf();

   psnes_init();
   if (strlen(g_extern.basename) > 0)
      psnes_set_cartridge_basename(g_extern.basename);

   SSNES_LOG("Version of libsnes API: %u.%u\n", psnes_library_revision_major(), psnes_library_revision_minor());

   fill_pathnames();

   if (!init_rom_file(g_extern.game_type))
      goto error;

#ifdef HAVE_XML
   init_cheats();
#endif

   init_msg_queue();
   init_movie();

   if (!g_extern.bsv_movie)
      load_save_files();

#ifdef HAVE_NETPLAY
   init_netplay();
#endif
   init_drivers();

   if (!g_extern.netplay)
      init_rewind();

#ifdef HAVE_NETPLAY
   psnes_set_video_refresh(g_extern.netplay ? video_frame_net : video_frame);
   psnes_set_audio_sample(g_extern.netplay ? audio_sample_net : audio_sample);
   psnes_set_input_state(g_extern.netplay ? input_state_net : input_state);
#else
   psnes_set_video_refresh(video_frame);
   psnes_set_audio_sample(audio_sample);
   psnes_set_input_state(input_state);
#endif
   psnes_set_input_poll(input_poll);
   
   init_controllers();
   
#ifdef HAVE_FFMPEG
   init_recording();
#endif

   if (!g_extern.bsv_movie_playback && !g_extern.netplay_is_client)
      init_autosave();

   // Main loop
   for(;;)
   {
      // DSP plugin GUI events.
      if (g_extern.audio_data.dsp_handle && g_extern.audio_data.dsp_plugin->events)
         g_extern.audio_data.dsp_plugin->events(g_extern.audio_data.dsp_handle);

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

#ifdef HAVE_NETPLAY
         if (g_extern.netplay)
            netplay_pre_frame(g_extern.netplay);
#endif
         if (g_extern.bsv_movie)
            bsv_movie_set_frame_start(g_extern.bsv_movie);

         psnes_run();

         if (g_extern.bsv_movie)
            bsv_movie_set_frame_end(g_extern.bsv_movie);
#ifdef HAVE_NETPLAY
         if (g_extern.netplay)
            netplay_post_frame(g_extern.netplay);
#endif

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

#ifdef HAVE_NETPLAY
   deinit_netplay();
#endif

   if (!g_extern.bsv_movie_playback && !g_extern.netplay_is_client)
      deinit_autosave();

#ifdef HAVE_FFMPEG
   deinit_recording();
#endif

   if (!g_extern.bsv_movie_playback && !g_extern.netplay_is_client)
      save_files();

   if (!g_extern.netplay)
      deinit_rewind();

#ifdef HAVE_XML
   deinit_cheats();
#endif

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



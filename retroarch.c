/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <boolean.h>
#include "libretro_version_1.h"
#include "dynamic.h"
#include "content.h"
#include "configuration.h"
#include <file/file_path.h>
#include <file/dir_list.h>
#include "general.h"
#include "retroarch.h"
#include "runloop.h"
#include "runloop_data.h"
#include <compat/strl.h>
#include "screenshot.h"
#include "performance.h"
#include "cheats.h"
#include <compat/getopt.h>
#include <compat/posix_string.h>

#include "input/keyboard_line.h"
#include "input/input_remapping.h"

#include "record/record_driver.h"

#include "git_version.h"
#include "intl/intl.h"

#ifdef HAVE_MENU
#include "menu/menu.h"
#include "menu/menu_shader.h"
#include "menu/menu_input.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#endif

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#endif

/**
 * rarch_render_cached_frame:
 *
 * Renders the current video frame.
 **/
void rarch_render_cached_frame(void)
{
   driver_t *driver   = driver_get_ptr();
   global_t *global   = global_get_ptr();
   runloop_t *runloop = rarch_main_get_ptr();
   void *recording    = driver ? driver->recording_data : NULL;

   if (runloop->is_idle)
      return;

   /* Cannot allow recording when pushing duped frames. */
   driver->recording_data = NULL;

   /* Not 100% safe, since the library might have
    * freed the memory, but no known implementations do this.
    * It would be really stupid at any rate ...
    */
   if (driver->retro_ctx.frame_cb)
      driver->retro_ctx.frame_cb(
            (global->frame_cache.data == RETRO_HW_FRAME_BUFFER_VALID)
            ? NULL : global->frame_cache.data,
            global->frame_cache.width,
            global->frame_cache.height,
            global->frame_cache.pitch);

   driver->recording_data = recording;
}

#include "config.features.h"

#define _PSUPP(var, name, desc) printf("\t%s:\n\t\t%s: %s\n", name, desc, _##var##_supp ? "yes" : "no")
static void print_features(void)
{
   puts("");
   puts("Features:");
   _PSUPP(sdl, "SDL", "SDL drivers");
   _PSUPP(sdl2, "SDL2", "SDL2 drivers");
   _PSUPP(x11, "X11", "X11 drivers");
   _PSUPP(wayland, "wayland", "Wayland drivers");
   _PSUPP(thread, "Threads", "Threading support");
   _PSUPP(opengl, "OpenGL", "OpenGL driver");
   _PSUPP(kms, "KMS", "KMS/EGL context support");
   _PSUPP(udev, "UDEV", "UDEV/EVDEV input driver support");
   _PSUPP(egl, "EGL", "EGL context support");
   _PSUPP(vg, "OpenVG", "OpenVG output support");
   _PSUPP(xvideo, "XVideo", "XVideo output");
   _PSUPP(alsa, "ALSA", "audio driver");
   _PSUPP(oss, "OSS", "audio driver");
   _PSUPP(jack, "Jack", "audio driver");
   _PSUPP(rsound, "RSound", "audio driver");
   _PSUPP(roar, "RoarAudio", "audio driver");
   _PSUPP(pulse, "PulseAudio", "audio driver");
   _PSUPP(dsound, "DirectSound", "audio driver");
   _PSUPP(xaudio, "XAudio2", "audio driver");
   _PSUPP(zlib, "zlib", "PNG encode/decode and .zip extraction");
   _PSUPP(al, "OpenAL", "audio driver");
   _PSUPP(dylib, "External", "External filter and plugin support");
   _PSUPP(cg, "Cg", "Cg pixel shaders");
   _PSUPP(libxml2, "libxml2", "libxml2 XML parsing");
   _PSUPP(sdl_image, "SDL_image", "SDL_image image loading");
   _PSUPP(fbo, "FBO", "OpenGL render-to-texture (multi-pass shaders)");
   _PSUPP(dynamic, "Dynamic", "Dynamic run-time loading of libretro library");
   _PSUPP(ffmpeg, "FFmpeg", "On-the-fly recording of gameplay with libavcodec");
   _PSUPP(freetype, "FreeType", "TTF font rendering with FreeType");
   _PSUPP(netplay, "Netplay", "Peer-to-peer netplay");
   _PSUPP(python, "Python", "Script support in shaders");
}
#undef _PSUPP

/**
 * print_compiler:
 *
 * Prints compiler that was used for compiling RetroArch.
 **/
static void print_compiler(FILE *file)
{
   fprintf(file, "\nCompiler: ");
#if defined(_MSC_VER)
   fprintf(file, "MSVC (%d) %u-bit\n", _MSC_VER, (unsigned)
         (CHAR_BIT * sizeof(size_t)));
#elif defined(__SNC__)
   fprintf(file, "SNC (%d) %u-bit\n",
      __SN_VER__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(_WIN32) && defined(__GNUC__)
   fprintf(file, "MinGW (%d.%d.%d) %u-bit\n",
      __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
      (CHAR_BIT * sizeof(size_t)));
#elif defined(__clang__)
   fprintf(file, "Clang/LLVM (%s) %u-bit\n",
      __clang_version__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(__GNUC__)
   fprintf(file, "GCC (%d.%d.%d) %u-bit\n",
      __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
      (CHAR_BIT * sizeof(size_t)));
#else
   fprintf(file, "Unknown compiler %u-bit\n",
      (unsigned)(CHAR_BIT * sizeof(size_t)));
#endif
   fprintf(file, "Built: %s\n", __DATE__);
}

/**
 * print_help:
 *
 * Prints help message explaining RetroArch's commandline switches.
 **/
static void print_help(void)
{
   puts("===================================================================");
#ifdef HAVE_GIT_VERSION
   printf(RETRO_FRONTEND ": Frontend for libretro -- v" PACKAGE_VERSION " -- %s --\n", rarch_git_version);
#else
   puts(RETRO_FRONTEND ": Frontend for libretro -- v" PACKAGE_VERSION " --");
#endif
   print_compiler(stdout);
   puts("===================================================================");
   puts("Usage: retroarch [content file] [options...]");
   puts("\t-h/--help: Show this help message.");
   puts("\t--menu: Do not require content or libretro core to be loaded, starts directly in menu.");
   puts("\t\tIf no arguments are passed to " RETRO_FRONTEND ", it is equivalent to using --menu as only argument.");
   puts("\t--features: Prints available features compiled into " RETRO_FRONTEND ".");
   puts("\t-s/--save: Path for save file (*.srm).");
   puts("\t-f/--fullscreen: Start " RETRO_FRONTEND " in fullscreen regardless of config settings.");
   puts("\t-S/--savestate: Path to use for save states. If not selected, *.state will be assumed.");
   puts("\t-c/--config: Path for config file." RARCH_DEFAULT_CONF_PATH_STR);
   puts("\t--appendconfig: Extra config files are loaded in, and take priority over config selected in -c (or default).");
   puts("\t\tMultiple configs are delimited by '|'.");
#ifdef HAVE_DYNAMIC
   puts("\t-L/--libretro: Path to libretro implementation. Overrides any config setting.");
#endif
   puts("\t--subsystem: Use a subsystem of the libretro core. Multiple content files are loaded as multiple arguments.");
   puts("\t\tIf a content file is skipped, use a blank (\"\") command line argument");
   puts("\t\tContent must be loaded in an order which depends on the particular subsystem used.");
   puts("\t\tSee verbose log output to learn how a particular subsystem wants content to be loaded.");

   printf("\t-N/--nodevice: Disconnects controller device connected to port (1 to %d).\n", MAX_USERS);
   printf("\t-A/--dualanalog: Connect a DualAnalog controller to port (1 to %d).\n", MAX_USERS);
   printf("\t-d/--device: Connect a generic device into port of the device (1 to %d).\n", MAX_USERS);
   puts("\t\tFormat is port:ID, where ID is an unsigned number corresponding to the particular device.\n");

   puts("\t-P/--bsvplay: Playback a BSV movie file.");
   puts("\t-R/--bsvrecord: Start recording a BSV movie file from the beginning.");
   puts("\t--eof-exit: Exit upon reaching the end of the BSV movie file.");
   puts("\t-M/--sram-mode: Takes an argument telling how SRAM should be handled in the session.");
   puts("\t\t{no,}load-{no,}save describes if SRAM should be loaded, and if SRAM should be saved.");
   puts("\t\tDo note that noload-save implies that save files will be deleted and overwritten.");

#ifdef HAVE_NETPLAY
   puts("\t-H/--host: Host netplay as user 1.");
   puts("\t-C/--connect: Connect to netplay as user 2.");
   puts("\t--port: Port used to netplay. Default is 55435.");
   puts("\t-F/--frames: Sync frames when using netplay.");
   puts("\t--spectate: Netplay will become spectating mode.");
   puts("\t\tHost can live stream the game content to users that connect.");
   puts("\t\tHowever, the client will not be able to play. Multiple clients can connect to the host.");
#endif
   puts("\t--nick: Picks a username (for use with netplay). Not mandatory.");
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
   puts("\t--command: Sends a command over UDP to an already running " RETRO_FRONTEND " process.");
   puts("\t\tAvailable commands are listed if command is invalid.");
#endif

   puts("\t-r/--record: Path to record video file.\n\t\tUsing .mkv extension is recommended.");
   puts("\t--recordconfig: Path to settings used during recording.");
   puts("\t--size: Overrides output video size when recording (format: WIDTHxHEIGHT).");
   puts("\t-v/--verbose: Verbose logging.");
   puts("\t-U/--ups: Specifies path for UPS patch that will be applied to content.");
   puts("\t--bps: Specifies path for BPS patch that will be applied to content.");
   puts("\t--ips: Specifies path for IPS patch that will be applied to content.");
   puts("\t--no-patch: Disables all forms of content patching.");
   puts("\t-D/--detach: Detach " RETRO_FRONTEND " from the running console. Not relevant for all platforms.");
   puts("\t--max-frames: Runs for the specified number of frames, then exits.\n");
}

static void set_basename(const char *path)
{
   char *dst          = NULL;
   global_t *global   = global_get_ptr();

   strlcpy(global->fullpath, path, sizeof(global->fullpath));
   strlcpy(global->basename, path, sizeof(global->basename));

#ifdef HAVE_COMPRESSION
   /* Removing extension is a bit tricky for compressed files.
    * Basename means:
    * /file/to/path/game.extension should be:
    * /file/to/path/game
    *
    * Two things to consider here are: /file/to/path/ is expected
    * to be a directory and "game" is a single file. This is used for
    * states and srm default paths.
    *
    * For compressed files we have:
    *
    * /file/to/path/comp.7z#game.extension and
    * /file/to/path/comp.7z#folder/game.extension
    *
    * The choice I take here is:
    * /file/to/path/game as basename. We might end up in a writable 
    * directory then and the name of srm and states are meaningful.
    *
    */
   path_basedir(global->basename);
   fill_pathname_dir(global->basename, path, "", sizeof(global->basename));
#endif

   if ((dst = strrchr(global->basename, '.')))
      *dst = '\0';
}

static void set_special_paths(char **argv, unsigned num_content)
{
   unsigned i;
   union string_list_elem_attr attr;
   global_t   *global   = global_get_ptr();
   settings_t *settings = config_get_ptr();

   /* First content file is the significant one. */
   set_basename(argv[0]);

   global->subsystem_fullpaths = string_list_new();
   rarch_assert(global->subsystem_fullpaths);

   attr.i = 0;

   for (i = 0; i < num_content; i++)
      string_list_append(global->subsystem_fullpaths, argv[i], attr);

   /* We defer SRAM path updates until we can resolve it.
    * It is more complicated for special content types. */

   if (!global->has_set_state_path)
      fill_pathname_noext(global->savestate_name, global->basename,
            ".state", sizeof(global->savestate_name));

   if (path_is_directory(global->savestate_name))
   {
      fill_pathname_dir(global->savestate_name, global->basename,
            ".state", sizeof(global->savestate_name));
      RARCH_LOG("Redirecting save state to \"%s\".\n",
            global->savestate_name);
   }

   /* If this is already set,
    * do not overwrite it as this was initialized before in
    * a menu or otherwise. */
   if (!*settings->system_directory)
      fill_pathname_basedir(settings->system_directory, argv[0],
            sizeof(settings->system_directory));
}

static void set_paths_redirect(const char *path)
{
   global_t   *global   = global_get_ptr();

   if (path_is_directory(global->savefile_name))
   {
      fill_pathname_dir(global->savefile_name, global->basename,
            ".srm", sizeof(global->savefile_name));
      RARCH_LOG("Redirecting save file to \"%s\".\n", global->savefile_name);
   }

   if (path_is_directory(global->savestate_name))
   {
      fill_pathname_dir(global->savestate_name, global->basename,
            ".state", sizeof(global->savestate_name));
      RARCH_LOG("Redirecting save state to \"%s\".\n", global->savestate_name);
   }

   if (path_is_directory(global->cheatfile_name))
   {
      fill_pathname_dir(global->cheatfile_name, global->basename,
            ".state", sizeof(global->cheatfile_name));
      RARCH_LOG("Redirecting cheat file to \"%s\".\n", global->cheatfile_name);
   }
}

static void set_paths(const char *path)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   set_basename(path);

   if (!global->has_set_save_path)
      fill_pathname_noext(global->savefile_name, global->basename,
            ".srm", sizeof(global->savefile_name));
   if (!global->has_set_state_path)
      fill_pathname_noext(global->savestate_name, global->basename,
            ".state", sizeof(global->savestate_name));
   fill_pathname_noext(global->cheatfile_name, global->basename,
         ".cht", sizeof(global->cheatfile_name));

   set_paths_redirect(path);

   /* If this is already set, do not overwrite it
    * as this was initialized before in a menu or otherwise. */
   if (*settings->system_directory)
      return;

   fill_pathname_basedir(settings->system_directory, path,
         sizeof(settings->system_directory));
}

/**
 * parse_input:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments. 
 *
 * Parses (commandline) arguments passed to RetroArch.
 *
 **/
static void parse_input(int argc, char *argv[])
{
   runloop_t *runloop = rarch_main_get_ptr();
   global_t  *global  = global_get_ptr();

   global->libretro_no_content           = false;
   global->libretro_dummy                = false;
   global->has_set_save_path             = false;
   global->has_set_state_path            = false;
   global->has_set_libretro              = false;
   global->has_set_libretro_directory    = false;
   global->has_set_verbosity             = false;

   global->has_set_netplay_mode          = false;
   global->has_set_username              = false;
   global->has_set_netplay_ip_address    = false;
   global->has_set_netplay_delay_frames  = false;
   global->has_set_netplay_ip_port       = false;

   global->has_set_ups_pref              = false;
   global->has_set_bps_pref              = false;
   global->has_set_ips_pref              = false;

   global->ups_pref                      = false;
   global->bps_pref                      = false;
   global->ips_pref                      = false;
   *global->ups_name                     = '\0';
   *global->bps_name                     = '\0';
   *global->ips_name                     = '\0';
   *global->subsystem                    = '\0';
   
   global->overrides_active              = false; 

   if (argc < 2)
   {
      global->libretro_dummy             = true;
      return;
   }

   /* Make sure we can call parse_input several times ... */
   optind = 0;

   int val = 0;

   const struct option opts[] = {
#ifdef HAVE_DYNAMIC
      { "libretro", 1, NULL, 'L' },
#endif
      { "menu", 0, &val, 'M' },
      { "help", 0, NULL, 'h' },
      { "save", 1, NULL, 's' },
      { "fullscreen", 0, NULL, 'f' },
      { "record", 1, NULL, 'r' },
      { "recordconfig", 1, &val, 'R' },
      { "size", 1, &val, 's' },
      { "verbose", 0, NULL, 'v' },
      { "config", 1, NULL, 'c' },
      { "appendconfig", 1, &val, 'C' },
      { "nodevice", 1, NULL, 'N' },
      { "dualanalog", 1, NULL, 'A' },
      { "device", 1, NULL, 'd' },
      { "savestate", 1, NULL, 'S' },
      { "bsvplay", 1, NULL, 'P' },
      { "bsvrecord", 1, NULL, 'R' },
      { "sram-mode", 1, NULL, 'M' },
#ifdef HAVE_NETPLAY
      { "host", 0, NULL, 'H' },
      { "connect", 1, NULL, 'C' },
      { "frames", 1, NULL, 'F' },
      { "port", 1, &val, 'p' },
      { "spectate", 0, &val, 'S' },
#endif
      { "nick", 1, &val, 'N' },
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
      { "command", 1, &val, 'c' },
#endif
      { "ups", 1, NULL, 'U' },
      { "bps", 1, &val, 'B' },
      { "ips", 1, &val, 'I' },
      { "no-patch", 0, &val, 'n' },
      { "detach", 0, NULL, 'D' },
      { "features", 0, &val, 'f' },
      { "subsystem", 1, NULL, 'Z' },
      { "max-frames", 1, NULL, 'm' },
      { "eof-exit", 0, &val, 'e' },
      { NULL, 0, NULL, 0 }
   };

#define FFMPEG_RECORD_ARG "r:"

#ifdef HAVE_DYNAMIC
#define DYNAMIC_ARG "L:"
#else
#define DYNAMIC_ARG
#endif

#ifdef HAVE_NETPLAY
#define NETPLAY_ARG "HC:F:"
#else
#define NETPLAY_ARG
#endif


#define BSV_MOVIE_ARG "P:R:M:"

   const char *optstring = "hs:fvS:A:c:U:DN:d:" BSV_MOVIE_ARG NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG;
   settings_t *settings = config_get_ptr();

   for (;;)
   {
      int port;
      val   = 0;
      int c = getopt_long(argc, argv, optstring, opts, NULL);

      if (c == -1)
         break;

      switch (c)
      {
         case 'h':
            print_help();
            exit(0);

         case 'Z':
            strlcpy(global->subsystem, optarg, sizeof(global->subsystem));
            break;

         case 'd':
         {
            unsigned id = 0;
            struct string_list *list = string_split(optarg, ":");

            port = 0;

            if (list && list->size == 2)
            {
               port = strtol(list->elems[0].data, NULL, 0);
               id = strtoul(list->elems[1].data, NULL, 0);
            }
            string_list_free(list);

            if (port < 1 || port > MAX_USERS)
            {
               RARCH_ERR("Connect device to a valid port.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            settings->input.libretro_device[port - 1] = id;
            global->has_set_libretro_device[port - 1] = true;
            break;
         }

         case 'A':
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > MAX_USERS)
            {
               RARCH_ERR("Connect dualanalog to a valid port.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            settings->input.libretro_device[port - 1] = RETRO_DEVICE_ANALOG;
            global->has_set_libretro_device[port - 1] = true;
            break;

         case 's':
            strlcpy(global->savefile_name, optarg,
                  sizeof(global->savefile_name));
            global->has_set_save_path = true;
            break;

         case 'f':
            global->force_fullscreen = true;
            break;

         case 'S':
            strlcpy(global->savestate_name, optarg,
                  sizeof(global->savestate_name));
            global->has_set_state_path = true;
            break;

         case 'v':
            global->verbosity = true;
            global->has_set_verbosity = true;
            break;

         case 'N':
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > MAX_USERS)
            {
               RARCH_ERR("Disconnect device from a valid port.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            settings->input.libretro_device[port - 1] = RETRO_DEVICE_NONE;
            global->has_set_libretro_device[port - 1] = true;
            break;

         case 'c':
            strlcpy(global->config_path, optarg,
                  sizeof(global->config_path));
            break;

         case 'r':
            strlcpy(global->record.path, optarg,
                  sizeof(global->record.path));
            global->record.enable = true;
            break;

#ifdef HAVE_DYNAMIC
         case 'L':
            if (path_is_directory(optarg))
            {
               *settings->libretro = '\0';
               strlcpy(settings->libretro_directory, optarg,
                     sizeof(settings->libretro_directory));
               global->has_set_libretro = true;
               global->has_set_libretro_directory = true;
               RARCH_WARN("Using old --libretro behavior. Setting libretro_directory to \"%s\" instead.\n", optarg);
            }
            else
            {
               strlcpy(settings->libretro, optarg,
                     sizeof(settings->libretro));
               global->has_set_libretro = true;
            }
            break;
#endif
         case 'P':
         case 'R':
            strlcpy(global->bsv.movie_start_path, optarg,
                  sizeof(global->bsv.movie_start_path));
            global->bsv.movie_start_playback  = (c == 'P');
            global->bsv.movie_start_recording = (c == 'R');
            break;

         case 'M':
            if (strcmp(optarg, "noload-nosave") == 0)
            {
               global->sram_load_disable = true;
               global->sram_save_disable = true;
            }
            else if (strcmp(optarg, "noload-save") == 0)
               global->sram_load_disable = true;
            else if (strcmp(optarg, "load-nosave") == 0)
               global->sram_save_disable = true;
            else if (strcmp(optarg, "load-save") != 0)
            {
               RARCH_ERR("Invalid argument in --sram-mode.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            break;

#ifdef HAVE_NETPLAY
         case 'H':
            global->has_set_netplay_ip_address = true;
            global->netplay_enable = true;
            *global->netplay_server = '\0';
            break;

         case 'C':
            global->has_set_netplay_ip_address = true;
            global->netplay_enable = true;
            strlcpy(global->netplay_server, optarg,
                  sizeof(global->netplay_server));
            break;

         case 'F':
            global->netplay_sync_frames = strtol(optarg, NULL, 0);
            global->has_set_netplay_delay_frames = true;
            break;
#endif

         case 'U':
            strlcpy(global->ups_name, optarg,
                  sizeof(global->ups_name));
            global->ups_pref = true;
            global->has_set_ups_pref = true;
            break;

         case 'D':
#if defined(_WIN32) && !defined(_XBOX)
            FreeConsole();
#endif
            break;

         case 'm':
            runloop->frames.video.max = strtoul(optarg, NULL, 10);
            break;

         case 0:
            switch (val)
            {
               case 'M':
                  global->libretro_dummy = true;
                  break;

#ifdef HAVE_NETPLAY
               case 'p':
                  global->has_set_netplay_ip_port = true;
                  global->netplay_port = strtoul(optarg, NULL, 0);
                  break;

               case 'S':
                  global->has_set_netplay_mode = true;
                  global->netplay_is_spectate = true;
                  break;

#endif
               case 'N':
                  global->has_set_username = true;
                  strlcpy(settings->username, optarg,
                        sizeof(settings->username));
                  break;

#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
               case 'c':
                  if (network_cmd_send(optarg))
                     exit(0);
                  else
                     rarch_fail(1, "network_cmd_send()");
                  break;
#endif

               case 'C':
                  strlcpy(global->append_config_path, optarg,
                        sizeof(global->append_config_path));
                  break;

               case 'B':
                  strlcpy(global->bps_name, optarg,
                        sizeof(global->bps_name));
                  global->bps_pref = true;
                  global->has_set_bps_pref = true;
                  break;

               case 'I':
                  strlcpy(global->ips_name, optarg,
                        sizeof(global->ips_name));
                  global->ips_pref = true;
                  global->has_set_ips_pref = true;
                  break;

               case 'n':
                  global->block_patch = true;
                  break;

               case 's':
               {
                  if (sscanf(optarg, "%ux%u", &global->record.width,
                           &global->record.height) != 2)
                  {
                     RARCH_ERR("Wrong format for --size.\n");
                     print_help();
                     rarch_fail(1, "parse_input()");
                  }
                  break;
               }

               case 'R':
                  strlcpy(global->record.config, optarg,
                        sizeof(global->record.config));
                  break;
               case 'f':
                  print_features();
                  exit(0);

               case 'e':
                  global->bsv.eof_exit = true;
                  break;

               default:
                  break;
            }
            break;

         case '?':
            print_help();
            rarch_fail(1, "parse_input()");

         default:
            RARCH_ERR("Error parsing arguments.\n");
            rarch_fail(1, "parse_input()");
      }
   }

   if (global->libretro_dummy)
   {
      if (optind < argc)
      {
         RARCH_ERR("--menu was used, but content file was passed as well.\n");
         rarch_fail(1, "parse_input()");
      }
   }
   else if (!*global->subsystem && optind < argc)
      set_paths(argv[optind]);
   else if (*global->subsystem && optind < argc)
      set_special_paths(argv + optind, argc - optind);
   else
      global->libretro_no_content = true;

   /* Copy SRM/state dirs used, so they can be reused on reentrancy. */
   if (global->has_set_save_path &&
         path_is_directory(global->savefile_name))
      strlcpy(global->savefile_dir, global->savefile_name,
            sizeof(global->savefile_dir));

   if (global->has_set_state_path &&
         path_is_directory(global->savestate_name))
      strlcpy(global->savestate_dir, global->savestate_name,
            sizeof(global->savestate_dir));
}

/**
 * init_controllers:
 *
 * Initialize libretro controllers.
 **/
static void init_controllers(void)
{
   unsigned i;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   for (i = 0; i < MAX_USERS; i++)
   {
      const char *ident = NULL;
      const struct retro_controller_description *desc = NULL;
      unsigned device = settings->input.libretro_device[i];

      if (i < global->system.num_ports)
         desc = libretro_find_controller_description(
               &global->system.ports[i], device);

      if (desc)
         ident = desc->desc;

      if (!ident)
      {
         /* If we're trying to connect a completely unknown device,
          * revert back to JOYPAD. */
         
         if (device != RETRO_DEVICE_JOYPAD && device != RETRO_DEVICE_NONE)
         {
            /* Do not fix settings->input.libretro_device[i],
             * because any use of dummy core will reset this,
             * which is not a good idea. */
            RARCH_WARN("Input device ID %u is unknown to this libretro implementation. Using RETRO_DEVICE_JOYPAD.\n", device);
            device = RETRO_DEVICE_JOYPAD;
         }
         ident = "Joypad";
      }

      switch (device)
      {
         case RETRO_DEVICE_NONE:
            RARCH_LOG("Disconnecting device from port %u.\n", i + 1);
            pretro_set_controller_port_device(i, device);
            break;
         case RETRO_DEVICE_JOYPAD:
            break;
         default:
            /* Some cores do not properly range check port argument.
             * This is broken behavior of course, but avoid breaking
             * cores needlessly. */
            RARCH_LOG("Connecting %s (ID: %u) to port %u.\n", ident,
                  device, i + 1);
            pretro_set_controller_port_device(i, device);
            break;
      }
   }
}

static bool load_save_files(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   if (!global->savefiles || global->sram_load_disable)
      return false;

   for (i = 0; i < global->savefiles->size; i++)
      load_ram_file(global->savefiles->elems[i].data,
            global->savefiles->elems[i].attr.i);
    
   return true;
}

static void save_files(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   if (!global->savefiles || !global->use_sram)
      return;

   for (i = 0; i < global->savefiles->size; i++)
   {
      unsigned type    = global->savefiles->elems[i].attr.i;
      const char *path = global->savefiles->elems[i].data;
      RARCH_LOG("Saving RAM type #%u to \"%s\".\n", type, path);
      save_ram_file(path, type);
   }
}

static void init_remapping(void)
{
   settings_t *settings = config_get_ptr();

   if (!settings->input.remap_binds_enable)
      return;

   input_remapping_load_file(settings->input.remapping_path);
}

static void init_cheats(void)
{
   bool allow_cheats = true;
   driver_t *driver  = driver_get_ptr();
   global_t *global  = global_get_ptr();

   (void)driver;

#ifdef HAVE_NETPLAY
   allow_cheats &= !driver->netplay_data;
#endif
   allow_cheats &= !global->bsv.movie;

   if (!allow_cheats)
      return;

   /* TODO/FIXME - add some stuff here. */
}

static void init_movie(void)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (global->bsv.movie_start_playback)
   {
      if (!(global->bsv.movie = bsv_movie_init(global->bsv.movie_start_path,
                  RARCH_MOVIE_PLAYBACK)))
      {
         RARCH_ERR("Failed to load movie file: \"%s\".\n",
               global->bsv.movie_start_path);
         rarch_fail(1, "init_movie()");
      }

      global->bsv.movie_playback = true;
      rarch_main_msg_queue_push("Starting movie playback.", 2, 180, false);
      RARCH_LOG("Starting movie playback.\n");
      settings->rewind_granularity = 1;
   }
   else if (global->bsv.movie_start_recording)
   {
      char msg[PATH_MAX_LENGTH];
      snprintf(msg, sizeof(msg), "Starting movie record to \"%s\".",
            global->bsv.movie_start_path);

      if (!(global->bsv.movie = bsv_movie_init(global->bsv.movie_start_path,
                  RARCH_MOVIE_RECORD)))
      {
         rarch_main_msg_queue_push("Failed to start movie record.", 1, 180, true);
         RARCH_ERR("Failed to start movie record.\n");
         return;
      }

      rarch_main_msg_queue_push(msg, 1, 180, true);
      RARCH_LOG("Starting movie record to \"%s\".\n",
            global->bsv.movie_start_path);
      settings->rewind_granularity = 1;
   }
}

#ifdef HAVE_COMMAND
static void init_command(void)
{
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!settings->stdin_cmd_enable && !settings->network_cmd_enable)
      return;

   if (settings->stdin_cmd_enable && driver->stdin_claimed)
   {
      RARCH_WARN("stdin command interface is desired, but input driver has already claimed stdin.\n"
            "Cannot use this command interface.\n");
   }

   if (!(driver->command = rarch_cmd_new(settings->stdin_cmd_enable
               && !driver->stdin_claimed,
               settings->network_cmd_enable, settings->network_cmd_port)))
      RARCH_ERR("Failed to initialize command interface.\n");
}
#endif

#if defined(HAVE_THREADS)
static void init_autosave(void)
{
   unsigned i;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (settings->autosave_interval < 1 || !global->savefiles)
      return;

   if (!(global->autosave = (autosave_t**)calloc(global->savefiles->size,
               sizeof(*global->autosave))))
      return;

   global->num_autosave = global->savefiles->size;

   for (i = 0; i < global->savefiles->size; i++)
   {
      const char *path = global->savefiles->elems[i].data;
      unsigned    type = global->savefiles->elems[i].attr.i;

      if (pretro_get_memory_size(type) <= 0)
         continue;

      global->autosave[i] = autosave_new(path,
            pretro_get_memory_data(type),
            pretro_get_memory_size(type),
            settings->autosave_interval);

      if (!global->autosave[i])
         RARCH_WARN(RETRO_LOG_INIT_AUTOSAVE_FAILED);
   }
}

static void deinit_autosave(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   for (i = 0; i < global->num_autosave; i++)
      autosave_free(global->autosave[i]);

   if (global->autosave)
      free(global->autosave);
   global->autosave     = NULL;

   global->num_autosave = 0;
}
#endif

static void set_savestate_auto_index(void)
{
   char state_dir[PATH_MAX_LENGTH], state_base[PATH_MAX_LENGTH];
   size_t i;
   struct string_list *dir_list = NULL;
   unsigned max_idx             = 0;
   settings_t *settings         = config_get_ptr();
   global_t   *global           = global_get_ptr();

   if (!settings->savestate_auto_index)
      return;

   /* Find the file in the same directory as global->savestate_name
    * with the largest numeral suffix.
    *
    * E.g. /foo/path/content.state, will try to find
    * /foo/path/content.state%d, where %d is the largest number available.
    */

   fill_pathname_basedir(state_dir, global->savestate_name,
         sizeof(state_dir));
   fill_pathname_base(state_base, global->savestate_name,
         sizeof(state_base));

   if (!(dir_list = dir_list_new(state_dir, NULL, false)))
      return;

   for (i = 0; i < dir_list->size; i++)
   {
      unsigned idx;
      char elem_base[PATH_MAX_LENGTH];
      const char *end      = NULL;
      const char *dir_elem = dir_list->elems[i].data;

      fill_pathname_base(elem_base, dir_elem, sizeof(elem_base));

      if (strstr(elem_base, state_base) != elem_base)
         continue;

      end = dir_elem + strlen(dir_elem);
      while ((end > dir_elem) && isdigit(end[-1]))
         end--;

      idx = strtoul(end, NULL, 0);
      if (idx > max_idx)
         max_idx = idx;
   }

   dir_list_free(dir_list);

   settings->state_slot = max_idx;
   RARCH_LOG("Found last state slot: #%d\n", settings->state_slot);
}

static void rarch_init_savefile_paths(void)
{
   global_t *global = global_get_ptr();

   rarch_main_command(EVENT_CMD_SAVEFILES_DEINIT);

   global->savefiles = string_list_new();
   rarch_assert(global->savefiles);

   if (*global->subsystem)
   {
      /* For subsystems, we know exactly which RAM types are supported. */

      unsigned i, j;
      const struct retro_subsystem_info *info = 
         libretro_find_subsystem_info(
               global->system.special, global->system.num_special,
               global->subsystem);

      /* We'll handle this error gracefully later. */
      unsigned num_content = min(info ? info->num_roms : 0,
            global->subsystem_fullpaths ?
            global->subsystem_fullpaths->size : 0);

      bool use_sram_dir = path_is_directory(global->savefile_name);

      for (i = 0; i < num_content; i++)
      {
         for (j = 0; j < info->roms[i].num_memory; j++)
         {
            union string_list_elem_attr attr;
            char path[PATH_MAX_LENGTH], ext[32];
            const struct retro_subsystem_memory_info *mem =
               (const struct retro_subsystem_memory_info*)
               &info->roms[i].memory[j];

            snprintf(ext, sizeof(ext), ".%s", mem->extension);

            if (use_sram_dir)
            {
               /* Redirect content fullpath to save directory. */
               strlcpy(path, global->savefile_name, sizeof(path));
               fill_pathname_dir(path,
                     global->subsystem_fullpaths->elems[i].data, ext,
                     sizeof(path));
            }
            else
            {
               fill_pathname(path, global->subsystem_fullpaths->elems[i].data,
                     ext, sizeof(path));
            }

            attr.i = mem->type;
            string_list_append(global->savefiles, path, attr);
         }
      }

      /* Let other relevant paths be inferred from the main SRAM location. */
      if (!global->has_set_save_path)
         fill_pathname_noext(global->savefile_name, global->basename, ".srm",
               sizeof(global->savefile_name));
      if (path_is_directory(global->savefile_name))
      {
         fill_pathname_dir(global->savefile_name, global->basename, ".srm",
               sizeof(global->savefile_name));
         RARCH_LOG("Redirecting save file to \"%s\".\n",
               global->savefile_name);
      }
   }
   else
   {
      char savefile_name_rtc[PATH_MAX_LENGTH];
      union string_list_elem_attr attr;

      attr.i = RETRO_MEMORY_SAVE_RAM;
      string_list_append(global->savefiles, global->savefile_name, attr);

      /* Infer .rtc save path from save ram path. */
      attr.i = RETRO_MEMORY_RTC;
      fill_pathname(savefile_name_rtc,
            global->savefile_name, ".rtc", sizeof(savefile_name_rtc));
      string_list_append(global->savefiles, savefile_name_rtc, attr);
   }
}

static void fill_pathnames(void)
{
   global_t   *global   = global_get_ptr();

   rarch_init_savefile_paths();
   fill_pathname(global->bsv.movie_path, global->savefile_name, "",
         sizeof(global->bsv.movie_path));

   if (!*global->basename)
      return;

   if (!*global->ups_name)
      fill_pathname_noext(global->ups_name, global->basename, ".ups",
            sizeof(global->ups_name));
   if (!*global->bps_name)
      fill_pathname_noext(global->bps_name, global->basename, ".bps",
            sizeof(global->bps_name));
   if (!*global->ips_name)
      fill_pathname_noext(global->ips_name, global->basename, ".ips",
            sizeof(global->ips_name));
}

static void load_auto_state(void)
{
   bool ret;
   char msg[PATH_MAX_LENGTH];
   char savestate_name_auto[PATH_MAX_LENGTH];
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

#ifdef HAVE_NETPLAY
   if (global->netplay_enable && !global->netplay_is_spectate)
      return;
#endif

   if (!settings->savestate_auto_load)
      return;

   fill_pathname_noext(savestate_name_auto, global->savestate_name,
         ".auto", sizeof(savestate_name_auto));

   if (!path_file_exists(savestate_name_auto))
      return;

   ret = load_state(savestate_name_auto);

   RARCH_LOG("Found auto savestate in: %s\n", savestate_name_auto);

   snprintf(msg, sizeof(msg), "Auto-loading savestate from \"%s\" %s.",
         savestate_name_auto, ret ? "succeeded" : "failed");
   rarch_main_msg_queue_push(msg, 1, 180, false);
   RARCH_LOG("%s\n", msg);
}

static bool save_auto_state(void)
{
   bool ret;
   char savestate_name_auto[PATH_MAX_LENGTH];
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (!settings->savestate_auto_save || global->libretro_dummy ||
       global->libretro_no_content)
       return false;

   fill_pathname_noext(savestate_name_auto, global->savestate_name,
         ".auto", sizeof(savestate_name_auto));

   ret = save_state(savestate_name_auto);
   RARCH_LOG("Auto save state to \"%s\" %s.\n", savestate_name_auto, ret ?
         "succeeded" : "failed");
    
   return true;
}

/**
 * rarch_load_state
 * @path            : Path to state.
 * @msg             : Message.
 * @sizeof_msg      : Size of @msg.
 *
 * Loads a state with path being @path.
 **/
static void rarch_load_state(const char *path,
      char *msg, size_t sizeof_msg)
{
   settings_t *settings = config_get_ptr();

   if (!load_state(path))
   {
      snprintf(msg, sizeof_msg,
            "Failed to load state from \"%s\".", path);
      return;

   }

   if (settings->state_slot < 0)
      snprintf(msg, sizeof_msg,
            "Loaded state from slot #-1 (auto).");
   else
      snprintf(msg, sizeof_msg,
            "Loaded state from slot #%d.", settings->state_slot);
}

/**
 * rarch_save_state
 * @path            : Path to state.
 * @msg             : Message.
 * @sizeof_msg      : Size of @msg.
 *
 * Saves a state with path being @path.
 **/
static void rarch_save_state(const char *path,
      char *msg, size_t sizeof_msg)
{
   settings_t *settings = config_get_ptr();

   if (!save_state(path))
   {
      snprintf(msg, sizeof_msg,
            "Failed to save state to \"%s\".", path);
      return;
   }

   if (settings->state_slot < 0)
      snprintf(msg, sizeof_msg,
            "Saved state to slot #-1 (auto).");
   else
      snprintf(msg, sizeof_msg,
            "Saved state to slot #%d.", settings->state_slot);
}

static void main_state(unsigned cmd)
{
   char path[PATH_MAX_LENGTH], msg[PATH_MAX_LENGTH];
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();

   if (settings->state_slot > 0)
      snprintf(path, sizeof(path), "%s%d",
            global->savestate_name, settings->state_slot);
   else if (settings->state_slot < 0)
      snprintf(path, sizeof(path), "%s.auto",
            global->savestate_name);
   else
      strlcpy(path, global->savestate_name, sizeof(path));

   if (pretro_serialize_size())
   {
      if (cmd == EVENT_CMD_SAVE_STATE)
         rarch_save_state(path, msg, sizeof(msg));
      else if (cmd == EVENT_CMD_LOAD_STATE)
         rarch_load_state(path, msg, sizeof(msg));
   }
   else
      strlcpy(msg, "Core does not support save states.", sizeof(msg));

   rarch_main_msg_queue_push(msg, 2, 180, true);
   RARCH_LOG("%s\n", msg);
}

/**
 * rarch_disk_control_append_image:
 * @path                 : Path to disk image. 
 *
 * Appends disk image to disk image list.
 **/
void rarch_disk_control_append_image(const char *path)
{
   char msg[PATH_MAX_LENGTH];
   unsigned new_idx;
   struct retro_game_info info = {0};
   global_t *global = global_get_ptr();
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&global->system.disk_control;

   rarch_disk_control_set_eject(true, false);

   control->add_image_index();
   new_idx = control->get_num_images();
   if (!new_idx)
      return;
   new_idx--;

   info.path = path;
   control->replace_image_index(new_idx, &info);

   snprintf(msg, sizeof(msg), "Appended disk: %s", path);
   RARCH_LOG("%s\n", msg);
   rarch_main_msg_queue_push(msg, 0, 180, true);

   rarch_main_command(EVENT_CMD_AUTOSAVE_DEINIT);

   /* TODO: Need to figure out what to do with subsystems case. */
   if (!*global->subsystem)
   {
      /* Update paths for our new image.
       * If we actually use append_image, we assume that we
       * started out in a single disk case, and that this way
       * of doing it makes the most sense. */
      set_paths(path);
      fill_pathnames();
   }

   rarch_main_command(EVENT_CMD_AUTOSAVE_INIT);

   rarch_disk_control_set_eject(false, false);
}

/**
 * rarch_disk_control_set_eject:
 * @new_state            : Eject or close the virtual drive tray.
 *                         false (0) : Close
 *                         true  (1) : Eject
 * @print_log            : Show message onscreen.
 *
 * Ejects/closes of the virtual drive tray.
 **/
void rarch_disk_control_set_eject(bool new_state, bool print_log)
{
   char msg[PATH_MAX_LENGTH];
   global_t *global = global_get_ptr();
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&global->system.disk_control;
   bool error = false;

   if (!control->get_num_images)
      return;

   *msg = '\0';

   if (control->set_eject_state(new_state))
      snprintf(msg, sizeof(msg), "%s virtual disk tray.",
            new_state ? "Ejected" : "Closed");
   else
   {
      error = true;
      snprintf(msg, sizeof(msg), "Failed to %s virtual disk tray.",
            new_state ? "eject" : "close");
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);

      /* Only noise in menu. */
      if (print_log)
         rarch_main_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * rarch_disk_control_set_index:
 * @idx                : Index of disk to set as current.
 *
 * Sets current disk to @index.
 **/
void rarch_disk_control_set_index(unsigned idx)
{
   char msg[PATH_MAX_LENGTH];
   unsigned num_disks;
   global_t *global = global_get_ptr();
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&global->system.disk_control;
   bool error = false;

   if (!control->get_num_images)
      return;

   *msg = '\0';

   num_disks = control->get_num_images();

   if (control->set_image_index(idx))
   {
      if (idx < num_disks)
         snprintf(msg, sizeof(msg), "Setting disk %u of %u in tray.",
               idx + 1, num_disks);
      else
         strlcpy(msg, "Removed disk from tray.", sizeof(msg));
   }
   else
   {
      if (idx < num_disks)
         snprintf(msg, sizeof(msg), "Failed to set disk %u of %u.",
               idx + 1, num_disks);
      else
         strlcpy(msg, "Failed to remove disk from tray.", sizeof(msg));
      error = true;
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);
      rarch_main_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * check_disk_eject:
 * @control              : Handle to disk control handle.
 *
 * Perform disk eject (Core Disk Options).
 **/
static void check_disk_eject(
      const struct retro_disk_control_callback *control)
{
   bool new_state = !control->get_eject_state();
   rarch_disk_control_set_eject(new_state, true);
}

/**
 * check_disk_next:
 * @control              : Handle to disk control handle.
 *
 * Perform disk cycle to next index action (Core Disk Options).
 **/
static void check_disk_next(
      const struct retro_disk_control_callback *control)
{
   unsigned num_disks        = control->get_num_images();
   unsigned current          = control->get_image_index();
   bool     disk_next_enable = num_disks && num_disks != UINT_MAX;

   if (!disk_next_enable)
   {
      RARCH_ERR("Got invalid disk index from libretro.\n");
      return;
   }

   if (current < num_disks - 1)
      current++;
   rarch_disk_control_set_index(current);
}

/**
 * check_disk_prev:
 * @control              : Handle to disk control handle.
 *
 * Perform disk cycle to previous index action (Core Disk Options).
 **/
static void check_disk_prev(
      const struct retro_disk_control_callback *control)
{
   unsigned num_disks    = control->get_num_images();
   unsigned current      = control->get_image_index();
   bool disk_prev_enable = num_disks && num_disks != UINT_MAX;

   if (!disk_prev_enable)
   {
      RARCH_ERR("Got invalid disk index from libretro.\n");
      return;
   }

   if (current > 0)
      current--;
   rarch_disk_control_set_index(current);
}

static bool init_state(void)
{
   driver_t *driver     = driver_get_ptr();
   if (!driver)
      return false;

   driver->video_active = true;
   driver->audio_active = true;

   return true;
}

/**
 * free_temporary_content:
 *
 * Frees temporary content handle.
 **/
static void free_temporary_content(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   for (i = 0; i < global->temporary_content->size; i++)
   {
      const char *path = global->temporary_content->elems[i].data;

      RARCH_LOG("Removing temporary content file: %s.\n", path);
      if (remove(path) < 0)
         RARCH_ERR("Failed to remove temporary file: %s.\n", path);
   }
   string_list_free(global->temporary_content);
}

static void main_clear_state_drivers(void)
{
   global_t *global = global_get_ptr();
   bool inited      = false;
   if (!global)
      return;
   inited           = global->main_is_init;
   if (!inited)
      return;

   rarch_main_command(EVENT_CMD_DRIVERS_DEINIT);
   rarch_main_command(EVENT_CMD_DRIVERS_INIT);
}

static void main_init_state_config(void)
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   for (i = 0; i < MAX_USERS; i++)
      settings->input.libretro_device[i] = RETRO_DEVICE_JOYPAD;
}

void rarch_main_alloc(void)
{
   settings_t *settings = config_get_ptr();

   if (settings)
      config_free();
   settings = config_init();

   if (!settings)
      return;

   rarch_main_command(EVENT_CMD_HISTORY_DEINIT);

   rarch_main_clear_state();
   rarch_main_data_clear_state();
}

/**
 * rarch_main_new:
 *
 * Will teardown drivers and clears all 
 * internal state of RetroArch.
 * If @inited is true, will initialize all
 * drivers again after teardown.
 **/
void rarch_main_new(void)
{
   main_clear_state_drivers();
   init_state();
   main_init_state_config();

   rarch_main_command(EVENT_CMD_MSG_QUEUE_INIT);
}

void rarch_main_free(void)
{
   rarch_main_command(EVENT_CMD_MSG_QUEUE_DEINIT);
   rarch_main_command(EVENT_CMD_LOG_FILE_DEINIT);
   rarch_main_command(EVENT_CMD_DRIVERS_DEINIT);

   rarch_main_state_free();
   rarch_main_global_free();
   config_free();
}

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

static void init_system_info(void)
{
   global_t *global               = global_get_ptr();
   struct retro_system_info *info = (struct retro_system_info*)
      global ? &global->system.info : NULL;

   pretro_get_system_info(info);

   if (!info->library_name)
      info->library_name = "Unknown";
   if (!info->library_version)
      info->library_version = "v0";

#ifdef RARCH_CONSOLE
   snprintf(global->title_buf, sizeof(global->title_buf), "%s %s",
         info->library_name, info->library_version);
#else
   snprintf(global->title_buf, sizeof(global->title_buf),
         RETRO_FRONTEND " : %s %s",
         info->library_name, info->library_version);
#endif
   strlcpy(global->system.valid_extensions, info->valid_extensions ?
         info->valid_extensions : DEFAULT_EXT,
         sizeof(global->system.valid_extensions));
   global->system.block_extract = info->block_extract;
}

/* 
 * verify_api_version:
 *
 * Compare libretro core API version against API version
 * used by RetroArch.
 *
 * TODO - when libretro v2 gets added, allow for switching
 * between libretro version backend dynamically.
 **/
static void verify_api_version(void)
{

   RARCH_LOG("Version of libretro API: %u\n", pretro_api_version());
   RARCH_LOG("Compiled against API: %u\n", RETRO_API_VERSION);

   if (pretro_api_version() != RETRO_API_VERSION)
      RARCH_WARN(RETRO_LOG_LIBRETRO_ABI_BREAK);
}

#define FAIL_CPU(simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   rarch_fail(1, "validate_cpu_features()"); \
} while(0)

/* validate_cpu_features:
 *
 * Validates CPU features for given processor architecture.
 *
 * Make sure we haven't compiled for something we cannot run.
 * Ideally, code would get swapped out depending on CPU support, 
 * but this will do for now.
 */
static void validate_cpu_features(void)
{
   uint64_t cpu = rarch_get_cpu_features();
   (void)cpu;

#ifdef __SSE__
   if (!(cpu & RETRO_SIMD_SSE))
      FAIL_CPU("SSE");
#endif
#ifdef __SSE2__
   if (!(cpu & RETRO_SIMD_SSE2))
      FAIL_CPU("SSE2");
#endif
#ifdef __AVX__
   if (!(cpu & RETRO_SIMD_AVX))
      FAIL_CPU("AVX");
#endif
}

/**
 * init_system_av_info:
 *
 * Initialize system A/V information by calling the libretro core's
 * get_system_av_info function.
 **/
static void init_system_av_info(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   global_t  *global  = global_get_ptr();

   pretro_get_system_av_info(&global->system.av_info);
   runloop->frames.limit.last_time = rarch_get_time_usec();
}

static void deinit_core(bool reinit)
{
   
   global_t *global = global_get_ptr();
   
   pretro_unload_game();
   pretro_deinit();

   if (reinit)
      rarch_main_command(EVENT_CMD_DRIVERS_DEINIT);
    
   if(global->overrides_active)
   {
      config_unload_override();
      global->overrides_active = false;
   }
   pretro_set_environment(rarch_environment_cb);
   uninit_libretro_sym();
}

static bool init_content(void)
{
   global_t *global = global_get_ptr();

   /* No content to be loaded for dummy core,
    * just successfully exit. */
   if (global->libretro_dummy) 
      return true;

   if (!global->libretro_no_content)
      fill_pathnames();

   if (!init_content_file())
      return false;

   if (global->libretro_no_content)
      return true;

   set_savestate_auto_index();

   if (load_save_files())
      RARCH_LOG("Skipping SRAM load.\n");

   load_auto_state();
   rarch_main_command(EVENT_CMD_BSV_MOVIE_INIT);
   rarch_main_command(EVENT_CMD_NETPLAY_INIT);

   return true;
}

static bool init_core(void)
{
   driver_t *driver = driver_get_ptr();
   global_t *global = global_get_ptr();   
   
   if (config_load_override())
      global->overrides_active = true;
   else
      global->overrides_active = false; 

   pretro_set_environment(rarch_environment_cb);  
  
   config_load_remap();

   verify_api_version();
   pretro_init();

   global->use_sram = !global->libretro_dummy &&
      !global->libretro_no_content;

   if (!init_content())
      return false;

   retro_init_libretro_cbs(&driver->retro_ctx);
   init_system_av_info();

   return true;
}

/**
 * rarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments. 
 *
 * Initializes RetroArch.
 *
 * Returns: 0 on success, otherwise 1 if there was an error.
 **/
int rarch_main_init(int argc, char *argv[])
{
   int sjlj_ret;
   global_t *global = global_get_ptr();

   init_state();

   if ((sjlj_ret = setjmp(global->error_sjlj_context)) > 0)
   {
      RARCH_ERR("Fatal error received in: \"%s\"\n", global->error_string);
      return sjlj_ret;
   }
   global->error_in_init = true;
   parse_input(argc, argv);

   if (global->verbosity)
   {
      RARCH_LOG_OUTPUT("=== Build =======================================");
      print_compiler(stderr);
      RARCH_LOG_OUTPUT("Version: %s\n", PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
      RARCH_LOG_OUTPUT("Git: %s\n", rarch_git_version);
#endif
      RARCH_LOG_OUTPUT("=================================================\n");
   }

   validate_cpu_features();
   config_load();

   init_libretro_sym(global->libretro_dummy);
   init_system_info();

   init_drivers_pre();

   if (!rarch_main_command(EVENT_CMD_CORE_INIT))
      goto error;

   rarch_main_command(EVENT_CMD_DRIVERS_INIT);
   rarch_main_command(EVENT_CMD_COMMAND_INIT);
   rarch_main_command(EVENT_CMD_REWIND_INIT);
   rarch_main_command(EVENT_CMD_CONTROLLERS_INIT);
   rarch_main_command(EVENT_CMD_RECORD_INIT);
   rarch_main_command(EVENT_CMD_CHEATS_INIT);
   rarch_main_command(EVENT_CMD_REMAPPING_INIT);

   rarch_main_command(EVENT_CMD_SAVEFILES_INIT);
#if defined(GEKKO) && defined(HW_RVL)
   {
      settings_t *settings = config_get_ptr();
       
      if (settings)
      {
         rarch_main_command(EVENT_CMD_VIDEO_SET_ASPECT_RATIO);
         video_driver_set_aspect_ratio(settings->video.aspect_ratio_idx);
      }
   }
#endif

   global->error_in_init = false;
   global->main_is_init  = true;
   return 0;

error:
   rarch_main_command(EVENT_CMD_CORE_DEINIT);

   global->main_is_init = false;
   return 1;
}

/**
 * rarch_main_init_wrap:
 * @args                 : Input arguments.
 * @argc                 : Count of arguments.
 * @argv                 : Arguments.
 *
 * Generates an @argc and @argv pair based on @args
 * of type rarch_main_wrap.
 **/
void rarch_main_init_wrap(const struct rarch_main_wrap *args,
      int *argc, char **argv)
{
   *argc = 0;
   argv[(*argc)++] = strdup("retroarch");

   if (!args->no_content)
   {
      if (args->content_path)
      {
         RARCH_LOG("Using content: %s.\n", args->content_path);
         argv[(*argc)++] = strdup(args->content_path);
      }
      else
      {
         RARCH_LOG("No content, starting dummy core.\n");
         argv[(*argc)++] = strdup("--menu");
      }
   }

   if (args->sram_path)
   {
      argv[(*argc)++] = strdup("-s");
      argv[(*argc)++] = strdup(args->sram_path);
   }

   if (args->state_path)
   {
      argv[(*argc)++] = strdup("-S");
      argv[(*argc)++] = strdup(args->state_path);
   }

   if (args->config_path)
   {
      argv[(*argc)++] = strdup("-c");
      argv[(*argc)++] = strdup(args->config_path);
   }

#ifdef HAVE_DYNAMIC
   if (args->libretro_path)
   {
      argv[(*argc)++] = strdup("-L");
      argv[(*argc)++] = strdup(args->libretro_path);
   }
#endif

   if (args->verbose)
      argv[(*argc)++] = strdup("-v");

#ifdef HAVE_FILE_LOGGER
   for (i = 0; i < *argc; i++)
      RARCH_LOG("arg #%d: %s\n", i, argv[i]);
#endif
}

void rarch_main_set_state(unsigned cmd)
{
   runloop_t *runloop   = rarch_main_get_ptr();
   driver_t *driver     = driver_get_ptr();
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();

   switch (cmd)
   {
      case RARCH_ACTION_STATE_MENU_RUNNING:
#ifdef HAVE_MENU
         {
            menu_handle_t *menu = menu_driver_get_ptr();
            if (!menu)
               return;

            menu_driver_toggle(true);

            /* Menu should always run with vsync on. */
            rarch_main_command(EVENT_CMD_VIDEO_SET_BLOCKING_STATE);
            /* Stop all rumbling before entering the menu. */
            rarch_main_command(EVENT_CMD_RUMBLE_STOP);

            if (settings->menu.pause_libretro)
               rarch_main_command(EVENT_CMD_AUDIO_STOP);

            /* Override keyboard callback to redirect to menu instead.
             * We'll use this later for something ...
             * FIXME: This should probably be moved to menu_common somehow. */
            if (global)
            {
               global->frontend_key_event = global->system.key_event;
               global->system.key_event   = menu_input_key_event;
               global->system.frame_time_last = 0;
            }

            menu->need_refresh = true;
            runloop->is_menu = true;
         }
#endif
         break;
      case RARCH_ACTION_STATE_LOAD_CONTENT:
#ifdef HAVE_MENU
         /* If content loading fails, we go back to menu. */
         if (!menu_load_content())
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING);
#endif
         if (driver->frontend_ctx && driver->frontend_ctx->content_loaded)
            driver->frontend_ctx->content_loaded();
         break;
      case RARCH_ACTION_STATE_MENU_RUNNING_FINISHED:
#ifdef HAVE_MENU
         menu_apply_deferred_settings();

         menu_driver_toggle(false);

         runloop->is_menu = false;

         driver_set_nonblock_state(driver->nonblock_state);

         if (settings && settings->menu.pause_libretro)
            rarch_main_command(EVENT_CMD_AUDIO_START);

         /* Prevent stray input from going to libretro core */
         driver->flushing_input = true;

         /* Restore libretro keyboard callback. */
         if (global)
            global->system.key_event = global->frontend_key_event;
#endif
         video_driver_set_texture_enable(false, false);
         break;
      case RARCH_ACTION_STATE_QUIT:
	     if (global)
            global->system.shutdown = true;
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         break;
      case RARCH_ACTION_STATE_FORCE_QUIT:
         if (global)
            global->lifecycle_state = 0;
         rarch_main_set_state(RARCH_ACTION_STATE_QUIT);
         break;
      case RARCH_ACTION_STATE_NONE:
      default:
         break;
   }
}

/**
 * save_core_config:
 *
 * Saves a new (core) configuration to a file. Filename is based
 * on heuristics to avoid typing.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
static bool save_core_config(void)
{
   char config_dir[PATH_MAX_LENGTH], config_name[PATH_MAX_LENGTH],
        config_path[PATH_MAX_LENGTH], msg[PATH_MAX_LENGTH];
   bool ret             = false;
   bool found_path      = false;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   *config_dir = '\0';

   if (*settings->menu_config_directory)
      strlcpy(config_dir, settings->menu_config_directory,
            sizeof(config_dir));
   else if (*global->config_path) /* Fallback */
      fill_pathname_basedir(config_dir, global->config_path,
            sizeof(config_dir));
   else
   {
      const char *message = "Config directory not set. Cannot save new config.";
      rarch_main_msg_queue_push(message, 1, 180, true);
      RARCH_ERR("%s\n", message);
      return false;
   }

   /* Infer file name based on libretro core. */
   if (*settings->libretro && path_file_exists(settings->libretro))
   {
      unsigned i;

      /* In case of collision, find an alternative name. */
      for (i = 0; i < 16; i++)
      {
         char tmp[64];

         fill_pathname_base(config_name, settings->libretro,
               sizeof(config_name));
         path_remove_extension(config_name);
         fill_pathname_join(config_path, config_dir, config_name,
               sizeof(config_path));

         *tmp = '\0';

         if (i)
            snprintf(tmp, sizeof(tmp), "-%u.cfg", i);
         else
            strlcpy(tmp, ".cfg", sizeof(tmp));

         strlcat(config_path, tmp, sizeof(config_path));

         if (!path_file_exists(config_path))
         {
            found_path = true;
            break;
         }
      }
   }

   /* Fallback to system time... */
   if (!found_path)
   {
      RARCH_WARN("Cannot infer new config path. Use current time.\n");
      fill_dated_filename(config_name, "cfg", sizeof(config_name));
      fill_pathname_join(config_path, config_dir, config_name,
            sizeof(config_path));
   }

   if ((ret = config_save_file(config_path)))
   {
      strlcpy(global->config_path, config_path,
            sizeof(global->config_path));
      snprintf(msg, sizeof(msg), "Saved new config to \"%s\".",
            config_path);
      RARCH_LOG("%s\n", msg);
   }
   else
   {
      snprintf(msg, sizeof(msg), "Failed saving config to \"%s\".",
            config_path);
      RARCH_ERR("%s\n", msg);
   }

   rarch_main_msg_queue_push(msg, 1, 180, true);

   return ret;
}

static bool rarch_update_system_info(struct retro_system_info *_info,
      bool *load_no_content)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

#if defined(HAVE_DYNAMIC)
   if (!(*settings->libretro))
      return false;

   libretro_get_system_info(settings->libretro, _info,
         load_no_content);
#endif
   if (!global->core_info)
      return false;

   if (!core_info_list_get_info(global->core_info,
            global->core_info_current, settings->libretro))
      return false;

   return true;
}

/**
 * set_volume:
 * @gain      : amount of gain to be applied to current volume level.
 *
 * Adjusts the current audio volume level.
 *
 **/
static void set_volume(float gain)
{
   char msg[PATH_MAX_LENGTH];
   settings_t *settings    = config_get_ptr();
   global_t   *global      = global_get_ptr();

   settings->audio.volume += gain;
   settings->audio.volume  = max(settings->audio.volume, -80.0f);
   settings->audio.volume  = min(settings->audio.volume, 12.0f);

   snprintf(msg, sizeof(msg), "Volume: %.1f dB", settings->audio.volume);
   rarch_main_msg_queue_push(msg, 1, 180, true);
   RARCH_LOG("%s\n", msg);

   global->audio_data.volume_gain = db_to_gain(settings->audio.volume);
}

/**
 * rarch_main_command:
 * @cmd                  : Command index.
 *
 * Performs RetroArch command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool rarch_main_command(unsigned cmd)
{
   unsigned i           = 0;
   bool boolean         = false;
   runloop_t *runloop   = rarch_main_get_ptr();
   driver_t  *driver    = driver_get_ptr();
   global_t  *global    = global_get_ptr();
   settings_t *settings = config_get_ptr();

   (void)i;

   switch (cmd)
   {
      case EVENT_CMD_LOAD_CONTENT_PERSIST:
#ifdef HAVE_DYNAMIC
         rarch_main_command(EVENT_CMD_LOAD_CORE);
#endif
         rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
         break;
      case EVENT_CMD_LOAD_CONTENT:
#ifdef HAVE_DYNAMIC
         rarch_main_command(EVENT_CMD_LOAD_CONTENT_PERSIST);
#else
         rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH,
               (void*)settings->libretro);
         rarch_environment_cb(RETRO_ENVIRONMENT_EXEC,
               (void*)global->fullpath);
         rarch_main_command(EVENT_CMD_QUIT);
#endif
         break;
      case EVENT_CMD_LOAD_CORE_DEINIT:
#ifdef HAVE_DYNAMIC
         libretro_free_system_info(&global->menu.info);
#endif
         break;
      case EVENT_CMD_LOAD_CORE_PERSIST:
         rarch_main_command(EVENT_CMD_LOAD_CORE_DEINIT);
         {
#ifdef HAVE_MENU
            menu_handle_t *menu = menu_driver_get_ptr();
            if (menu)
               rarch_update_system_info(&global->menu.info,
                     &menu->load_no_content);
#endif
         }
         break;
      case EVENT_CMD_LOAD_CORE:
         rarch_main_command(EVENT_CMD_LOAD_CORE_PERSIST);
#ifndef HAVE_DYNAMIC
         rarch_main_command(EVENT_CMD_QUIT);
#endif
         break;
      case EVENT_CMD_LOAD_STATE:
         /* Immutable - disallow savestate load when 
          * we absolutely cannot change game state. */
         if (global->bsv.movie)
            return false;

#ifdef HAVE_NETPLAY
         if (driver->netplay_data)
            return false;
#endif
         main_state(cmd);
         break;
      case EVENT_CMD_RESIZE_WINDOWED_SCALE:
         if (global->pending.windowed_scale == 0)
            return false;

         settings->video.scale = global->pending.windowed_scale;

         if (!settings->video.fullscreen)
            rarch_main_command(EVENT_CMD_REINIT);

         global->pending.windowed_scale = 0;
         break;
      case EVENT_CMD_MENU_TOGGLE:
         if (runloop->is_menu)
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         else
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING);
         break;
      case EVENT_CMD_CONTROLLERS_INIT:
         init_controllers();
         break;
      case EVENT_CMD_RESET:
         RARCH_LOG(RETRO_LOG_RESETTING_CONTENT);
         rarch_main_msg_queue_push("Reset.", 1, 120, true);
         pretro_reset();

         /* bSNES since v073r01 resets controllers to JOYPAD
          * after a reset, so just enforce it here. */
         rarch_main_command(EVENT_CMD_CONTROLLERS_INIT);
         break;
      case EVENT_CMD_SAVE_STATE:
         if (settings->savestate_auto_index)
            settings->state_slot++;

         main_state(cmd);
         break;
      case EVENT_CMD_TAKE_SCREENSHOT:
         if (!take_screenshot())
            return false;
         break;
      case EVENT_CMD_PREPARE_DUMMY:
         {
#ifdef HAVE_MENU
            menu_handle_t *menu = menu_driver_get_ptr();
            if (menu)
               menu->load_no_content = false;
#endif
            rarch_main_data_deinit();

            *global->fullpath = '\0';

            rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
            global->system.shutdown = false;
         }
         break;
      case EVENT_CMD_UNLOAD_CORE:
         rarch_main_command(EVENT_CMD_PREPARE_DUMMY);
#ifdef HAVE_DYNAMIC
         libretro_free_system_info(&global->menu.info);
#endif
         break;
      case EVENT_CMD_QUIT:
         rarch_main_set_state(RARCH_ACTION_STATE_QUIT);
         break;
      case EVENT_CMD_REINIT:
         driver->video_cache_context = 
            global->system.hw_render_callback.cache_context;
         driver->video_cache_context_ack = false;
         rarch_main_command(EVENT_CMD_RESET_CONTEXT);
         driver->video_cache_context = false;

         /* Poll input to avoid possibly stale data to corrupt things. */
         input_driver_poll();

#ifdef HAVE_MENU
         runloop->frames.video.current.menu.framebuf.dirty = true;
         if (runloop->is_menu)
             rarch_main_command(EVENT_CMD_VIDEO_SET_BLOCKING_STATE);
#endif
         break;
      case EVENT_CMD_CHEATS_DEINIT:
         if (!global)
            break;

         if (global->cheat)
            cheat_manager_free(global->cheat);
         global->cheat = NULL;
         break;
      case EVENT_CMD_CHEATS_INIT:
         rarch_main_command(EVENT_CMD_CHEATS_DEINIT);
         init_cheats();
         break;
      case EVENT_CMD_REMAPPING_DEINIT:
         break;
      case EVENT_CMD_REMAPPING_INIT:
         rarch_main_command(EVENT_CMD_REMAPPING_DEINIT);
         init_remapping();
         break;
      case EVENT_CMD_REWIND_DEINIT:
         if (!global)
            break;
#ifdef HAVE_NETPLAY
         if (driver->netplay_data)
            return false;
#endif
         if (global->rewind.state)
            state_manager_free(global->rewind.state);
         global->rewind.state = NULL;
         break;
      case EVENT_CMD_REWIND_INIT:
         init_rewind();
         break;
      case EVENT_CMD_REWIND_TOGGLE:
         if (settings->rewind_enable)
            rarch_main_command(EVENT_CMD_REWIND_INIT);
         else
            rarch_main_command(EVENT_CMD_REWIND_DEINIT);
         break;
      case EVENT_CMD_AUTOSAVE_DEINIT:
#ifdef HAVE_THREADS
         deinit_autosave();
#endif
         break;
      case EVENT_CMD_AUTOSAVE_INIT:
         rarch_main_command(EVENT_CMD_AUTOSAVE_DEINIT);
#ifdef HAVE_THREADS
         init_autosave();
#endif
         break;
      case EVENT_CMD_AUTOSAVE_STATE:
         save_auto_state();
         break;
      case EVENT_CMD_AUDIO_STOP:
         if (!driver->audio_data)
            return false;
         if (!audio_driver_alive())
            return false;

         if (!audio_driver_stop())
            return false;
         break;
      case EVENT_CMD_AUDIO_START:
         if (!driver->audio_data || audio_driver_alive())
            return false;

         if (!settings->audio.mute_enable && !audio_driver_start())
         {
            RARCH_ERR("Failed to start audio driver. Will continue without audio.\n");
            driver->audio_active = false;
         }
         break;
      case EVENT_CMD_AUDIO_MUTE_TOGGLE:
         {
            const char *msg = !settings->audio.mute_enable ?
               "Audio muted." : "Audio unmuted.";

            if (!audio_driver_mute_toggle())
            {
               RARCH_ERR("Failed to unmute audio.\n");
               return false;
            }

            rarch_main_msg_queue_push(msg, 1, 180, true);
            RARCH_LOG("%s\n", msg);
         }
         break;
      case EVENT_CMD_OVERLAY_DEINIT:
#ifdef HAVE_OVERLAY
         if (driver->overlay)
            input_overlay_free(driver->overlay);
         driver->overlay = NULL;

         memset(&driver->overlay_state, 0, sizeof(driver->overlay_state));
#endif
         break;
      case EVENT_CMD_OVERLAY_INIT:
         rarch_main_command(EVENT_CMD_OVERLAY_DEINIT);
#ifdef HAVE_OVERLAY
         if (driver->osk_enable)
         {
            if (!*settings->osk.overlay)
               break;
         }
         else
         {
            if (!*settings->input.overlay)
               break;
         }

         driver->overlay = input_overlay_new(driver->osk_enable ? settings->osk.overlay : settings->input.overlay,
               driver->osk_enable ? settings->osk.enable   : settings->input.overlay_enable,
               settings->input.overlay_opacity, settings->input.overlay_scale);
         if (!driver->overlay)
            RARCH_ERR("Failed to load overlay.\n");
#endif
         break;
      case EVENT_CMD_OVERLAY_NEXT:
#ifdef HAVE_OVERLAY
         input_overlay_next(driver->overlay, settings->input.overlay_opacity);
#endif
         break;
      case EVENT_CMD_DSP_FILTER_DEINIT:
         if (!global)
            break;

         if (global->audio_data.dsp)
            rarch_dsp_filter_free(global->audio_data.dsp);
         global->audio_data.dsp = NULL;
         break;
      case EVENT_CMD_DSP_FILTER_INIT:
         rarch_main_command(EVENT_CMD_DSP_FILTER_DEINIT);
         if (!*settings->audio.dsp_plugin)
            break;

         global->audio_data.dsp = rarch_dsp_filter_new(
               settings->audio.dsp_plugin, global->audio_data.in_rate);
         if (!global->audio_data.dsp)
            RARCH_ERR("[DSP]: Failed to initialize DSP filter \"%s\".\n",
                  settings->audio.dsp_plugin);
         break;
      case EVENT_CMD_GPU_RECORD_DEINIT:
         if (!global)
            break;

         if (global->record.gpu_buffer)
            free(global->record.gpu_buffer);
         global->record.gpu_buffer = NULL;
         break;
      case EVENT_CMD_RECORD_DEINIT:
         if (!recording_deinit())
            return false;
         break;
      case EVENT_CMD_RECORD_INIT:
         rarch_main_command(EVENT_CMD_HISTORY_DEINIT);
         if (!recording_init())
            return false;
         break;
      case EVENT_CMD_HISTORY_DEINIT:
         if (g_defaults.history)
            content_playlist_free(g_defaults.history);
         g_defaults.history = NULL;
         break;
      case EVENT_CMD_HISTORY_INIT:
         rarch_main_command(EVENT_CMD_HISTORY_DEINIT);
         if (!settings->history_list_enable)
            return false;
         RARCH_LOG("Loading history file: [%s].\n", settings->content_history_path);
         g_defaults.history = content_playlist_init(
               settings->content_history_path,
               settings->content_history_size);
         break;
      case EVENT_CMD_CORE_INFO_DEINIT:
         if (!global)
            break;

         if (global->core_info)
            core_info_list_free(global->core_info);
         global->core_info = NULL;
         break;
      case EVENT_CMD_DATA_RUNLOOP_FREE:
         rarch_main_data_free();
         break;
      case EVENT_CMD_CORE_INFO_INIT:
         rarch_main_command(EVENT_CMD_CORE_INFO_DEINIT);

         if (*settings->libretro_directory)
            global->core_info = core_info_list_new(settings->libretro_directory);
         break;
      case EVENT_CMD_CORE_DEINIT:
         deinit_core(true);
         break;
      case EVENT_CMD_CORE_INIT:
         if (!init_core())
            return false;
         break;
      case EVENT_CMD_VIDEO_APPLY_STATE_CHANGES:
         video_driver_apply_state_changes();
         break;
      case EVENT_CMD_VIDEO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case EVENT_CMD_VIDEO_SET_BLOCKING_STATE:
         video_driver_set_nonblock_state(boolean);
         break;
      case EVENT_CMD_VIDEO_SET_ASPECT_RATIO:
         video_driver_set_aspect_ratio(settings->video.aspect_ratio_idx);
         break;
      case EVENT_CMD_AUDIO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case EVENT_CMD_AUDIO_SET_BLOCKING_STATE:
         audio_driver_set_nonblock_state(boolean);
         break;
      case EVENT_CMD_OVERLAY_SET_SCALE_FACTOR:
#ifdef HAVE_OVERLAY
         input_overlay_set_scale_factor(driver->overlay,
               settings->input.overlay_scale);
#endif
         break;
      case EVENT_CMD_OVERLAY_SET_ALPHA_MOD:
#ifdef HAVE_OVERLAY
         input_overlay_set_alpha_mod(driver->overlay,
               settings->input.overlay_opacity);
#endif
         break;
      case EVENT_CMD_DRIVERS_DEINIT:
         uninit_drivers(DRIVERS_CMD_ALL);
         break;
      case EVENT_CMD_DRIVERS_INIT:
         init_drivers(DRIVERS_CMD_ALL);
         break;
      case EVENT_CMD_AUDIO_REINIT:
         uninit_drivers(DRIVER_AUDIO);
         init_drivers(DRIVER_AUDIO);
         break;
      case EVENT_CMD_RESET_CONTEXT:
         rarch_main_command(EVENT_CMD_DRIVERS_DEINIT);
         rarch_main_command(EVENT_CMD_DRIVERS_INIT);
         break;
      case EVENT_CMD_QUIT_RETROARCH:
         rarch_main_set_state(RARCH_ACTION_STATE_FORCE_QUIT);
         break;
      case EVENT_CMD_RESUME:
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         break;
      case EVENT_CMD_RESTART_RETROARCH:
#if defined(GEKKO) && defined(HW_RVL)
         fill_pathname_join(global->fullpath, g_defaults.core_dir,
               SALAMANDER_FILE,
               sizeof(global->fullpath));
#endif
         if (driver->frontend_ctx && driver->frontend_ctx->set_fork)
            driver->frontend_ctx->set_fork(true, false);
         break;
      case EVENT_CMD_MENU_SAVE_CONFIG:
         if (!save_core_config())
            return false;
         break;
      case EVENT_CMD_SHADERS_APPLY_CHANGES:
#ifdef HAVE_MENU
         menu_shader_manager_apply_changes();
#endif
         break;
      case EVENT_CMD_PAUSE_CHECKS:
         if (runloop->is_paused)
         {
            RARCH_LOG("Paused.\n");
            rarch_main_command(EVENT_CMD_AUDIO_STOP);

            if (settings->video.black_frame_insertion)
               rarch_render_cached_frame();
         }
         else
         {
            RARCH_LOG("Unpaused.\n");
            rarch_main_command(EVENT_CMD_AUDIO_START);
         }
         break;
      case EVENT_CMD_PAUSE_TOGGLE:
         runloop->is_paused = !runloop->is_paused;
         rarch_main_command(EVENT_CMD_PAUSE_CHECKS);
         break;
      case EVENT_CMD_UNPAUSE:
         runloop->is_paused = false;
         rarch_main_command(EVENT_CMD_PAUSE_CHECKS);
         break;
      case EVENT_CMD_PAUSE:
         runloop->is_paused = true;
         rarch_main_command(EVENT_CMD_PAUSE_CHECKS);
         break;
      case EVENT_CMD_MENU_PAUSE_LIBRETRO:
         if (runloop->is_menu)
         {
            if (settings->menu.pause_libretro)
               rarch_main_command(EVENT_CMD_AUDIO_STOP);
            else
               rarch_main_command(EVENT_CMD_AUDIO_START);
         }
         else
         {
            if (settings->menu.pause_libretro)
               rarch_main_command(EVENT_CMD_AUDIO_START);
         }
         break;
      case EVENT_CMD_SHADER_DIR_DEINIT:
         if (!global)
            break;

         dir_list_free(global->shader_dir.list);
         global->shader_dir.list = NULL;
         global->shader_dir.ptr  = 0;
         break;
      case EVENT_CMD_SHADER_DIR_INIT:
         rarch_main_command(EVENT_CMD_SHADER_DIR_DEINIT);

         if (!*settings->video.shader_dir)
            return false;

         global->shader_dir.list = dir_list_new(settings->video.shader_dir,
               "cg|cgp|glsl|glslp", false);

         if (!global->shader_dir.list || global->shader_dir.list->size == 0)
         {
            rarch_main_command(EVENT_CMD_SHADER_DIR_DEINIT);
            return false;
         }

         global->shader_dir.ptr  = 0;
         dir_list_sort(global->shader_dir.list, false);

         for (i = 0; i < global->shader_dir.list->size; i++)
            RARCH_LOG("Found shader \"%s\"\n",
                  global->shader_dir.list->elems[i].data);
         break;
      case EVENT_CMD_SAVEFILES:
         save_files();
         break;
      case EVENT_CMD_SAVEFILES_DEINIT:
         if (!global)
            break;

         if (global->savefiles)
            string_list_free(global->savefiles);
         global->savefiles = NULL;
         break;
      case EVENT_CMD_SAVEFILES_INIT:
         global->use_sram = global->use_sram && !global->sram_save_disable
#ifdef HAVE_NETPLAY
            && (!driver->netplay_data || !global->netplay_is_client)
#endif
            ;

         if (!global->use_sram)
            RARCH_LOG("SRAM will not be saved.\n");

         if (global->use_sram)
            rarch_main_command(EVENT_CMD_AUTOSAVE_INIT);
         break;
      case EVENT_CMD_MSG_QUEUE_DEINIT:
         rarch_main_msg_queue_free();
         break;
      case EVENT_CMD_MSG_QUEUE_INIT:
         rarch_main_command(EVENT_CMD_MSG_QUEUE_DEINIT);
         rarch_main_msg_queue_init();
         rarch_main_data_init_queues();
         break;
      case EVENT_CMD_BSV_MOVIE_DEINIT:
         if (!global)
            break;

         if (global->bsv.movie)
            bsv_movie_free(global->bsv.movie);
         global->bsv.movie = NULL;
         break;
      case EVENT_CMD_BSV_MOVIE_INIT:
         rarch_main_command(EVENT_CMD_BSV_MOVIE_DEINIT);
         init_movie();
         break;
      case EVENT_CMD_NETPLAY_DEINIT:
#ifdef HAVE_NETPLAY
         deinit_netplay();
#endif
         break;
      case EVENT_CMD_NETWORK_DEINIT:
#ifdef HAVE_NETWORKING
         network_deinit();
#endif
         break;
      case EVENT_CMD_NETWORK_INIT:
#ifdef HAVE_NETWORKING
         network_init();
#endif
         break;
      case EVENT_CMD_NETPLAY_INIT:
         rarch_main_command(EVENT_CMD_NETPLAY_DEINIT);
#ifdef HAVE_NETPLAY
         if (!init_netplay())
            return false;
#endif
         break;
      case EVENT_CMD_NETPLAY_FLIP_PLAYERS:
#ifdef HAVE_NETPLAY
         {
            netplay_t *netplay = (netplay_t*)driver->netplay_data;
            if (!netplay)
               return false;
            netplay_flip_users(netplay);
         }
#endif
         break;
      case EVENT_CMD_FULLSCREEN_TOGGLE:
         if (!video_driver_has_windowed())
            return false;

         /* If we go fullscreen we drop all drivers and 
          * reinitialize to be safe. */
         settings->video.fullscreen = !settings->video.fullscreen;
         rarch_main_command(EVENT_CMD_REINIT);
         break;
      case EVENT_CMD_COMMAND_DEINIT:
#ifdef HAVE_COMMAND
         if (driver->command)
            rarch_cmd_free(driver->command);
         driver->command = NULL;
#endif
         break;
      case EVENT_CMD_COMMAND_INIT:
         rarch_main_command(EVENT_CMD_COMMAND_DEINIT);

#ifdef HAVE_COMMAND
         init_command();
#endif
         break;
      case EVENT_CMD_TEMPORARY_CONTENT_DEINIT:
         if (!global)
            break;

         if (global->temporary_content)
            free_temporary_content();
         global->temporary_content = NULL;
         break;
      case EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT:
         if (!global)
            break;

         if (global->subsystem_fullpaths)
            string_list_free(global->subsystem_fullpaths);
         global->subsystem_fullpaths = NULL;
         break;
      case EVENT_CMD_LOG_FILE_DEINIT:
         if (!global)
            break;

         if (global->log_file)
            fclose(global->log_file);
         global->log_file = NULL;
         break;
      case EVENT_CMD_DISK_EJECT_TOGGLE:
         if (global->system.disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control = 
               (const struct retro_disk_control_callback*)
               &global->system.disk_control;

            if (control)
               check_disk_eject(control);
         }
         else
            rarch_main_msg_queue_push("Core does not support Disk Options.", 1, 120, true);
         break;
      case EVENT_CMD_DISK_NEXT:
         if (global->system.disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control = 
               (const struct retro_disk_control_callback*)
               &global->system.disk_control;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            check_disk_next(control);
         }
         else
            rarch_main_msg_queue_push("Core does not support Disk Options.", 1, 120, true);
         break;
      case EVENT_CMD_DISK_PREV:
         if (global->system.disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control = 
               (const struct retro_disk_control_callback*)
               &global->system.disk_control;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            check_disk_prev(control);
         }
         else
            rarch_main_msg_queue_push("Core does not support Disk Options.", 1, 120, true);
         break;
      case EVENT_CMD_RUMBLE_STOP:
         for (i = 0; i < MAX_USERS; i++)
         {
            input_driver_set_rumble_state(i, RETRO_RUMBLE_STRONG, 0);
            input_driver_set_rumble_state(i, RETRO_RUMBLE_WEAK, 0);
         }
         break;
      case EVENT_CMD_GRAB_MOUSE_TOGGLE:
         {
            static bool grab_mouse_state  = false;

            grab_mouse_state = !grab_mouse_state;

            if (!driver->input || !input_driver_grab_mouse(grab_mouse_state))
               return false;

            RARCH_LOG("Grab mouse state: %s.\n",
                  grab_mouse_state ? "yes" : "no");

            video_driver_show_mouse(!grab_mouse_state);
         }
         break;
      case EVENT_CMD_PERFCNT_REPORT_FRONTEND_LOG:
         rarch_perf_log();
         break;
      case EVENT_CMD_VOLUME_UP:
         set_volume(0.5f);
         break;
      case EVENT_CMD_VOLUME_DOWN:
         set_volume(-0.5f);
         break;
   }

   return true;
}

/**
 * rarch_main_deinit:
 *
 * Deinitializes RetroArch.
 **/
void rarch_main_deinit(void)
{
   global_t *global = global_get_ptr();

   rarch_main_command(EVENT_CMD_NETPLAY_DEINIT);
   rarch_main_command(EVENT_CMD_COMMAND_DEINIT);

   if (global->use_sram)
      rarch_main_command(EVENT_CMD_AUTOSAVE_DEINIT);

   rarch_main_command(EVENT_CMD_RECORD_DEINIT);
   rarch_main_command(EVENT_CMD_SAVEFILES);

   rarch_main_command(EVENT_CMD_REWIND_DEINIT);
   rarch_main_command(EVENT_CMD_CHEATS_DEINIT);
   rarch_main_command(EVENT_CMD_BSV_MOVIE_DEINIT);

   rarch_main_command(EVENT_CMD_AUTOSAVE_STATE);

   rarch_main_command(EVENT_CMD_CORE_DEINIT);

   rarch_main_command(EVENT_CMD_TEMPORARY_CONTENT_DEINIT);
   rarch_main_command(EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT);
   rarch_main_command(EVENT_CMD_SAVEFILES_DEINIT);

   global->main_is_init = false;
}

/**
 * rarch_playlist_load_content:
 * @playlist             : Playlist handle.
 * @idx                  : Index in playlist.
 *
 * Initializes core and loads content based on playlist entry.
 **/
void rarch_playlist_load_content(content_playlist_t *playlist,
      unsigned idx)
{
   const char *path      = NULL;
   const char *core_path = NULL;
   menu_handle_t *menu   = menu_driver_get_ptr();
   settings_t  *settings = config_get_ptr();

   content_playlist_get_index(playlist,
         idx, &path, &core_path, NULL);

   strlcpy(settings->libretro, core_path, sizeof(settings->libretro));

   if (menu)
      menu->load_no_content = (path) ? false : true;

   rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)path);

   rarch_main_command(EVENT_CMD_LOAD_CORE);
}

/**
 * rarch_defer_core:
 * @core_info            : Core info list handle.
 * @dir                  : Directory. Gets joined with @path.
 * @path                 : Path. Gets joined with @dir.
 * @menu_label           : Label identifier of menu setting.
 * @deferred_path        : Deferred core path. Will be filled in
 *                         by function.
 * @sizeof_deferred_path : Size of @deferred_path.
 *
 * Gets deferred core.
 *
 * Returns: 0 if there are multiple deferred cores and a 
 * selection needs to be made from a list, otherwise
 * returns -1 and fills in @deferred_path with path to core.
 **/
int rarch_defer_core(core_info_list_t *core_info, const char *dir,
      const char *path, const char *menu_label,
      char *deferred_path, size_t sizeof_deferred_path)
{
   char new_core_path[PATH_MAX_LENGTH];
   const core_info_t *info = NULL;
   size_t supported        = 0;
   settings_t *settings    = config_get_ptr();
   global_t   *global      = global_get_ptr();

   fill_pathname_join(deferred_path, dir, path, sizeof_deferred_path);

#ifdef HAVE_COMPRESSION
   if (path_is_compressed_file(dir))
   {
      /* In case of a compressed archive, we have to join with a hash */
      /* We are going to write at the position of dir: */
      rarch_assert(strlen(dir) < strlen(deferred_path));
      deferred_path[strlen(dir)] = '#';
   }
#endif

   if (core_info)
      core_info_list_get_supported_cores(core_info, deferred_path, &info,
            &supported);

   if (!strcmp(menu_label, "load_content"))
   {
      info = (const core_info_t*)&global->core_info_current;

      if (info)
      {
         strlcpy(new_core_path, info->path, sizeof(new_core_path));
         supported = 1;
      }
   }
   else
      strlcpy(new_core_path, info->path, sizeof(new_core_path));

   /* There are multiple deferred cores and a 
    * selection needs to be made from a list, return 0. */
   if (supported != 1)
      return 0;

   strlcpy(global->fullpath, deferred_path, sizeof(global->fullpath));

   if (path_file_exists(new_core_path))
      strlcpy(settings->libretro, new_core_path,
            sizeof(settings->libretro));
   return -1;
}

/**
 * rarch_replace_config:
 * @path                 : Path to config file to replace
 *                         current config file with.
 *
 * Replaces currently loaded configuration file with
 * another one. Will load a dummy core to flush state
 * properly.
 *
 * Quite intrusive and error prone.
 * Likely to have lots of small bugs.
 * Cleanly exit the main loop to ensure that all the tiny details
 * get set properly.
 *
 * This should mitigate most of the smaller bugs.
 *
 * Returns: true (1) if successful, false (0) if @path was the
 * same as the current config file.
 **/

bool rarch_replace_config(const char *path)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   /* If config file to be replaced is the same as the 
    * current config file, exit. */
   if (!strcmp(path, global->config_path))
      return false;

   if (settings->config_save_on_exit && *global->config_path)
      config_save_file(global->config_path);

   strlcpy(global->config_path, path, sizeof(global->config_path));
   global->block_config_read = false;
   *settings->libretro = '\0'; /* Load core in new config. */

   rarch_main_command(EVENT_CMD_PREPARE_DUMMY);

   return true;
}

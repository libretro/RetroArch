/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#endif

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>

#include <boolean.h>
#include <string/stdstring.h>
#include <lists/string_list.h>

#include <compat/strl.h>
#include <compat/getopt.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <retro_stat.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <rthreads/rthreads.h>
#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_COMMAND
#include "command.h"
#endif

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#endif

#include "config.features.h"
#include "content.h"
#include "core_type.h"
#include "core_info.h"
#include "dynamic.h"
#include "driver.h"
#include "input/input_config.h"
#include "msg_hash.h"
#include "movie.h"
#include "dirs.h"
#include "paths.h"
#include "file_path_special.h"
#include "verbosity.h"

#include "frontend/frontend_driver.h"
#include "audio/audio_driver.h"
#include "record/record_driver.h"
#include "core.h"
#include "configuration.h"
#include "runloop.h"
#include "managers/cheat_manager.h"
#include "tasks/tasks_internal.h"

#include "version.h"
#include "version_git.h"

#include "retroarch.h"

#include "command.h"

#define _PSUPP(var, name, desc) printf("  %s:\n\t\t%s: %s\n", name, desc, _##var##_supp ? "yes" : "no")

#define FAIL_CPU(simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   retroarch_fail(1, "validate_cpu_features()"); \
} while(0)

/* Descriptive names for options without short variant.
 *
 * Please keep the name in sync with the option name.
 * Order does not matter. */
enum
{
   RA_OPT_MENU = 256, /* must be outside the range of a char */
   RA_OPT_STATELESS,
   RA_OPT_CHECK_FRAMES,
   RA_OPT_PORT,
   RA_OPT_SPECTATE,
   RA_OPT_NICK,
   RA_OPT_COMMAND,
   RA_OPT_APPENDCONFIG,
   RA_OPT_BPS,
   RA_OPT_IPS,
   RA_OPT_NO_PATCH,
   RA_OPT_RECORDCONFIG,
   RA_OPT_SUBSYSTEM,
   RA_OPT_SIZE,
   RA_OPT_FEATURES,
   RA_OPT_VERSION,
   RA_OPT_EOF_EXIT,
   RA_OPT_LOG_FILE,
   RA_OPT_MAX_FRAMES
};

static jmp_buf error_sjlj_context;
static enum rarch_core_type current_core_type           = CORE_TYPE_PLAIN;
static enum rarch_core_type explicit_current_core_type  = CORE_TYPE_PLAIN;
static char error_string[255]                           = {0};

static retro_bits_t has_set_libretro_device;
static bool has_set_core                                = false;
static bool has_set_username                            = false;
static bool rarch_is_inited                             = false;
static bool rarch_error_on_init                         = false;
static bool rarch_block_config_read                     = false;
static bool rarch_force_fullscreen                      = false;
static bool has_set_verbosity                           = false;
static bool has_set_libretro                            = false;
static bool has_set_libretro_directory                  = false;
static bool has_set_save_path                           = false;
static bool has_set_state_path                          = false;
static bool has_set_netplay_mode                        = false;
static bool has_set_netplay_ip_address                  = false;
static bool has_set_netplay_ip_port                     = false;
static bool has_set_netplay_stateless_mode              = false;
static bool has_set_netplay_check_frames                = false;
static bool has_set_ups_pref                            = false;
static bool has_set_bps_pref                            = false;
static bool has_set_ips_pref                            = false;

static void retroarch_print_features(void)
{
   puts("");
   puts("Features:");

   _PSUPP(libretrodb,      "LibretroDB",      "LibretroDB support");
   _PSUPP(command,         "Command",         "Command interface support");
   _PSUPP(network_command, "Network Command", "Network Command interface "
         "support");

   _PSUPP(sdl,             "SDL",             "SDL input/audio/video drivers");
   _PSUPP(sdl2,            "SDL2",            "SDL2 input/audio/video drivers");
   _PSUPP(x11,             "X11",             "X11 input/video drivers");
   _PSUPP(wayland,         "wayland",         "Wayland input/video drivers");
   _PSUPP(thread,          "Threads",         "Threading support");

   _PSUPP(vulkan,          "Vulkan",          "Vulkan video driver");
   _PSUPP(opengl,          "OpenGL",          "OpenGL   video driver support");
   _PSUPP(opengles,        "OpenGL ES",       "OpenGLES video driver support");
   _PSUPP(xvideo,          "XVideo",          "Video driver");
   _PSUPP(udev,            "UDEV",            "UDEV/EVDEV input driver support");
   _PSUPP(egl,             "EGL",             "Video context driver");
   _PSUPP(kms,             "KMS",             "Video context driver");
   _PSUPP(vg,              "OpenVG",          "Video context driver");

   _PSUPP(coreaudio,       "CoreAudio",       "Audio driver");
   _PSUPP(alsa,            "ALSA",            "Audio driver");
   _PSUPP(oss,             "OSS",             "Audio driver");
   _PSUPP(jack,            "Jack",            "Audio driver");
   _PSUPP(rsound,          "RSound",          "Audio driver");
   _PSUPP(roar,            "RoarAudio",       "Audio driver");
   _PSUPP(pulse,           "PulseAudio",      "Audio driver");
   _PSUPP(dsound,          "DirectSound",     "Audio driver");
   _PSUPP(xaudio,          "XAudio2",         "Audio driver");
   _PSUPP(al,              "OpenAL",          "Audio driver");
   _PSUPP(sl,              "OpenSL",          "Audio driver");

   _PSUPP(7zip,            "7zip",            "7zip extraction support");
   _PSUPP(zlib,            "zlib",            ".zip extraction support");

   _PSUPP(dylib,           "External",        "External filter and plugin support");

   _PSUPP(cg,              "Cg",              "Fragment/vertex shader driver");
   _PSUPP(glsl,            "GLSL",            "Fragment/vertex shader driver");
   _PSUPP(glsl,            "HLSL",            "Fragment/vertex shader driver");

   _PSUPP(libxml2,         "libxml2",         "libxml2 XML parsing");

   _PSUPP(sdl_image,       "SDL_image",       "SDL_image image loading");
   _PSUPP(rpng,            "rpng",            "PNG image loading/encoding");
   _PSUPP(rpng,            "rjpeg",           "JPEG image loading");

   _PSUPP(fbo,             "FBO",             "OpenGL render-to-texture "
                                              "(multi-pass shaders)");
   
   _PSUPP(dynamic,         "Dynamic",         "Dynamic run-time loading of "
                                              "libretro library");
   _PSUPP(ffmpeg,          "FFmpeg",          "On-the-fly recording of gameplay "
                                              "with libavcodec");

   _PSUPP(freetype,        "FreeType",        "TTF font rendering driver");
   _PSUPP(coretext,        "CoreText",        "TTF font rendering driver "
                                              "(for OSX and/or iOS)");
   _PSUPP(netplay,         "Netplay",         "Peer-to-peer netplay");
   _PSUPP(python,          "Python",          "Script support in shaders");

   _PSUPP(libusb,          "Libusb",          "Libusb support");

   _PSUPP(cocoa,           "Cocoa",           "Cocoa UI companion support "
                                              "(for OSX and/or iOS)");

   _PSUPP(qt,              "Qt",              "Qt UI companion support");
   _PSUPP(avfoundation,    "AVFoundation",    "Camera driver");
   _PSUPP(v4l2,            "Video4Linux2",    "Camera driver");
}
#undef _PSUPP

static void retroarch_print_version(void)
{
   char str[255];

   str[0] = '\0';

   fprintf(stderr, "%s: %s -- v%s",
         msg_hash_to_str(MSG_PROGRAM),
         msg_hash_to_str(MSG_LIBRETRO_FRONTEND),
         PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
   printf(" -- %s --\n", retroarch_git_version);
#endif
   retroarch_get_capabilities(RARCH_CAPABILITIES_COMPILER, str, sizeof(str));
   fprintf(stdout, "%s", str);
   fprintf(stdout, "Built: %s\n", __DATE__);
}

/**
 * retroarch_print_help:
 *
 * Prints help message explaining the program's commandline switches.
 **/
static void retroarch_print_help(const char *arg0)
{
   puts("===================================================================");
   retroarch_print_version();
   puts("===================================================================");

   printf("Usage: %s [OPTIONS]... [FILE]\n", arg0);

   puts("  -h, --help            Show this help message.");
   puts("  -v, --verbose         Verbose logging.");
   puts("      --log-file=FILE   Log messages to FILE.");
   puts("      --version         Show version.");
   puts("      --features        Prints available features compiled into "
         "program.");
#ifdef HAVE_MENU
   puts("      --menu            Do not require content or libretro core to "
         "be loaded,\n"
        "                        starts directly in menu. If no arguments "
        "are passed to\n"
        "                        the program, it is equivalent to using "
        "--menu as only argument.");
#endif
   puts("  -s, --save=PATH       Path for save files (*.srm).");
   puts("  -S, --savestate=PATH  Path for the save state files (*.state).");
   puts("  -f, --fullscreen      Start the program in fullscreen regardless "
         "of config settings.");
   puts("  -c, --config=FILE     Path for config file."
#ifdef _WIN32
         "\n\t\tDefaults to retroarch.cfg in same directory as retroarch.exe."
         "\n\t\tIf a default config is not found, the program will attempt to"
         "create one."
#else
         "\n\t\tBy default looks for config in $XDG_CONFIG_HOME/retroarch/"
         "retroarch.cfg,\n\t\t$HOME/.config/retroarch/retroarch.cfg,\n\t\t"
         "and $HOME/.retroarch.cfg.\n\t\tIf a default config is not found, "
         "the program will attempt to create one based on the \n\t\t"
         "skeleton config (" GLOBAL_CONFIG_DIR "/retroarch.cfg). \n"
#endif
         );
   puts("      --appendconfig=FILE\n"
        "                        Extra config files are loaded in, "
        "and take priority over\n"
        "                        config selected in -c (or default). "
        "Multiple configs are\n"
        "                        delimited by '|'.");
#ifdef HAVE_DYNAMIC
   puts("  -L, --libretro=FILE   Path to libretro implementation. "
         "Overrides any config setting.");
#endif
   puts("      --subsystem=NAME  Use a subsystem of the libretro core. "
         "Multiple content\n"
        "                        files are loaded as multiple arguments. "
        "If a content\n"
        "                        file is skipped, use a blank (\"\") "
        "command line argument.\n"
        "                        Content must be loaded in an order "
        "which depends on the\n"
        "                        particular subsystem used. See verbose "
        "log output to learn\n"
        "                        how a particular subsystem wants content "
        "to be loaded.\n");

   printf("  -N, --nodevice=PORT\n"
          "                        Disconnects controller device connected "
          "to PORT (1 to %d).\n", MAX_USERS);
   printf("  -A, --dualanalog=PORT\n"
          "                        Connect a DualAnalog controller to PORT "
          "(1 to %d).\n", MAX_USERS);
   printf("  -d, --device=PORT:ID\n"
          "                        Connect a generic device into PORT of "
          "the device (1 to %d).\n", MAX_USERS);
   puts("                        Format is PORT:ID, where ID is a number "
         "corresponding to the particular device.");

   puts("  -P, --bsvplay=FILE    Playback a BSV movie file.");
   puts("  -R, --bsvrecord=FILE  Start recording a BSV movie file from "
         "the beginning.");
   puts("      --eof-exit        Exit upon reaching the end of the "
         "BSV movie file.");
   puts("  -M, --sram-mode=MODE  SRAM handling mode. MODE can be "
         "'noload-nosave',\n"
        "                        'noload-save', 'load-nosave' or "
        "'load-save'.\n"
        "                        Note: 'noload-save' implies that "
        "save files *WILL BE OVERWRITTEN*.");

#ifdef HAVE_NETWORKING
   puts("  -H, --host            Host netplay as user 1.");
   puts("  -C, --connect=HOST    Connect to netplay server as user 2.");
   puts("      --port=PORT       Port used to netplay. Default is 55435.");
   puts("      --stateless       Use \"stateless\" mode for netplay");
   puts("                        (requires a very fast network).");
   puts("      --check-frames=NUMBER\n"
        "                        Check frames when using netplay.");
#if defined(HAVE_NETWORK_CMD)
   puts("      --command         Sends a command over UDP to an already "
         "running program process.");
   puts("      Available commands are listed if command is invalid.");
#endif

#endif
   puts("      --nick=NICK       Picks a username (for use with netplay). "
         "Not mandatory.");

   puts("  -r, --record=FILE     Path to record video file.\n        "
         "Using .mkv extension is recommended.");
   puts("      --recordconfig    Path to settings used during recording.");
   puts("      --size=WIDTHxHEIGHT\n"
        "                        Overrides output video size when recording.");
   puts("  -U, --ups=FILE        Specifies path for UPS patch that will be "
         "applied to content.");
   puts("      --bps=FILE        Specifies path for BPS patch that will be "
         "applied to content.");
   puts("      --ips=FILE        Specifies path for IPS patch that will be "
         "applied to content.");
   puts("      --no-patch        Disables all forms of content patching.");
   puts("  -D, --detach          Detach program from the running console. "
         "Not relevant for all platforms.");
   puts("      --max-frames=NUMBER\n"
        "                        Runs for the specified number of frames, "
        "then exits.\n");
}

#define FFMPEG_RECORD_ARG "r:"

#ifdef HAVE_DYNAMIC
#define DYNAMIC_ARG "L:"
#else
#define DYNAMIC_ARG
#endif

#ifdef HAVE_NETWORKING
#define NETPLAY_ARG "HC:F:"
#else
#define NETPLAY_ARG
#endif

#define BSV_MOVIE_ARG "P:R:M:"

/**
 * retroarch_parse_input:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Parses (commandline) arguments passed to program.
 *
 **/
static void retroarch_parse_input(int argc, char *argv[])
{
   const char *optstring = NULL;
   bool explicit_menu    = false;
   global_t  *global     = global_get_ptr();

   const struct option opts[] = {
#ifdef HAVE_DYNAMIC
      { "libretro",     1, NULL, 'L' },
#endif
      { "menu",         0, NULL, RA_OPT_MENU },
      { "help",         0, NULL, 'h' },
      { "save",         1, NULL, 's' },
      { "fullscreen",   0, NULL, 'f' },
      { "record",       1, NULL, 'r' },
      { "recordconfig", 1, NULL, RA_OPT_RECORDCONFIG },
      { "size",         1, NULL, RA_OPT_SIZE },
      { "verbose",      0, NULL, 'v' },
      { "config",       1, NULL, 'c' },
      { "appendconfig", 1, NULL, RA_OPT_APPENDCONFIG },
      { "nodevice",     1, NULL, 'N' },
      { "dualanalog",   1, NULL, 'A' },
      { "device",       1, NULL, 'd' },
      { "savestate",    1, NULL, 'S' },
      { "bsvplay",      1, NULL, 'P' },
      { "bsvrecord",    1, NULL, 'R' },
      { "sram-mode",    1, NULL, 'M' },
#ifdef HAVE_NETWORKING
      { "host",         0, NULL, 'H' },
      { "connect",      1, NULL, 'C' },
      { "stateless",    0, NULL, RA_OPT_STATELESS },
      { "check-frames", 1, NULL, RA_OPT_CHECK_FRAMES },
      { "port",         1, NULL, RA_OPT_PORT },
#if defined(HAVE_NETWORK_CMD)
      { "command",      1, NULL, RA_OPT_COMMAND },
#endif
#endif
      { "nick",         1, NULL, RA_OPT_NICK },
      { "ups",          1, NULL, 'U' },
      { "bps",          1, NULL, RA_OPT_BPS },
      { "ips",          1, NULL, RA_OPT_IPS },
      { "no-patch",     0, NULL, RA_OPT_NO_PATCH },
      { "detach",       0, NULL, 'D' },
      { "features",     0, NULL, RA_OPT_FEATURES },
      { "subsystem",    1, NULL, RA_OPT_SUBSYSTEM },
      { "max-frames",   1, NULL, RA_OPT_MAX_FRAMES },
      { "eof-exit",     0, NULL, RA_OPT_EOF_EXIT },
      { "version",      0, NULL, RA_OPT_VERSION },
#ifdef HAVE_FILE_LOGGER
      { "log-file",     1, NULL, RA_OPT_LOG_FILE },
#endif
      { NULL, 0, NULL, 0 }
   };

   /* Handling the core type is finicky. Based on the arguments we pass in,
    * we handle it differently.
    * Some current cases which track desired behavior and how it is supposed to work:
    *
    * Dynamically linked RA:
    * ./retroarch                            -> CORE_TYPE_DUMMY
    * ./retroarch -v                         -> CORE_TYPE_DUMMY + verbose
    * ./retroarch --menu                     -> CORE_TYPE_DUMMY
    * ./retroarch --menu -v                  -> CORE_TYPE_DUMMY + verbose
    * ./retroarch -L contentless-core        -> CORE_TYPE_PLAIN
    * ./retroarch -L content-core            -> CORE_TYPE_PLAIN + FAIL (This currently crashes)
    * ./retroarch [-L content-core] ROM      -> CORE_TYPE_PLAIN
    * ./retroarch <-L or ROM> --menu         -> FAIL
    *
    * The heuristic here seems to be that if we use the -L CLI option or
    * optind < argc at the end we should set CORE_TYPE_PLAIN.
    * To handle --menu, we should ensure that CORE_TYPE_DUMMY is still set
    * otherwise, fail early, since the CLI options are non-sensical.
    * We could also simply ignore --menu in this case to be more friendly with
    * bogus arguments.
    */

   if (!has_set_core)
      retroarch_set_current_core_type(CORE_TYPE_DUMMY, false);

   path_clear(RARCH_PATH_SUBSYSTEM);

   retroarch_override_setting_free_state();

   rarch_ctl(RARCH_CTL_USERNAME_UNSET, NULL);
   rarch_ctl(RARCH_CTL_UNSET_UPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_IPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_BPS_PREF, NULL);
   *global->name.ups                     = '\0';
   *global->name.bps                     = '\0';
   *global->name.ips                     = '\0';

   runloop_ctl(RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE, NULL);

   /* Make sure we can call retroarch_parse_input several times ... */
   optind    = 0;
   optstring = "hs:fvS:A:c:U:DN:d:"
      BSV_MOVIE_ARG NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG;

#ifndef HAVE_MENU
   if (argc == 1)
   {
      printf("%s\n", msg_hash_to_str(MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN));
      retroarch_print_help(argv[0]);
      exit(0);
   }
#endif

   for (;;)
   {
      int port;
      int c = getopt_long(argc, argv, optstring, opts, NULL);

#if 0
      fprintf(stderr, "c is: %c (%d), optarg is: [%s]\n", c, c, string_is_empty(optarg) ? "" : optarg);
#endif

      if (c == -1)
         break;

      switch (c)
      {
         case 'h':
            retroarch_print_help(argv[0]);
            exit(0);

         case 'd':
         {
            unsigned new_port;
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
               RARCH_ERR("%s\n", msg_hash_to_str(MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT));
               retroarch_print_help(argv[0]);
               retroarch_fail(1, "retroarch_parse_input()");
            }
            new_port = port -1;

            input_config_set_device(new_port, id);

            retroarch_override_setting_set(
                  RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
            break;
         }

         case 'A':
         {
            unsigned new_port;
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > MAX_USERS)
            {
               RARCH_ERR("Connect dualanalog to a valid port.\n");
               retroarch_print_help(argv[0]);
               retroarch_fail(1, "retroarch_parse_input()");
            }
            new_port = port - 1;

            input_config_set_device(new_port, RETRO_DEVICE_ANALOG);
            retroarch_override_setting_set(
                  RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
         }
            break;

         case 's':
            strlcpy(global->name.savefile, optarg,
                  sizeof(global->name.savefile));
            retroarch_override_setting_set(
                  RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL);
            break;

         case 'f':
            rarch_ctl(RARCH_CTL_SET_FORCE_FULLSCREEN, NULL);
            break;

         case 'S':
            strlcpy(global->name.savestate, optarg,
                  sizeof(global->name.savestate));
            retroarch_override_setting_set(
                  RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
            break;

         case 'v':
            verbosity_enable();
            retroarch_override_setting_set(
                  RARCH_OVERRIDE_SETTING_VERBOSITY, NULL);
            break;

         case 'N':
            {
               unsigned new_port;
               port = strtol(optarg, NULL, 0);
               if (port < 1 || port > MAX_USERS)
               {
                  RARCH_ERR("%s\n",
                        msg_hash_to_str(MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT));
                  retroarch_print_help(argv[0]);
                  retroarch_fail(1, "retroarch_parse_input()");
               }
               new_port = port - 1;
               input_config_set_device(port - 1, RETRO_DEVICE_NONE);
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
            }
            break;

         case 'c':
            path_set(RARCH_PATH_CONFIG, optarg);
            break;

         case 'r':
            strlcpy(global->record.path, optarg,
                  sizeof(global->record.path));
            {
               bool *recording_enabled = recording_is_enabled();

               if (recording_enabled)
                  *recording_enabled = true;
            }
            break;

#ifdef HAVE_DYNAMIC
         case 'L':
            if (path_is_directory(optarg))
            {
               settings_t *settings  = config_get_ptr();

               path_clear(RARCH_PATH_CORE);
               strlcpy(settings->directory.libretro, optarg,
                     sizeof(settings->directory.libretro));

               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY, NULL);
               RARCH_WARN("Using old --libretro behavior. "
                     "Setting libretro_directory to \"%s\" instead.\n",
                     optarg);
            }
            else if (path_file_exists(optarg))
            {
               runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, optarg);
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);

               /* We requested explicit core, so use PLAIN core type. */
               retroarch_set_current_core_type(CORE_TYPE_PLAIN, false);
            }
            else
            {
               RARCH_WARN("--libretro argument \"%s\" is neither a file nor directory. Ignoring.\n",
                     optarg);
            }

            break;
#endif
         case 'P':
         case 'R':
            bsv_movie_set_start_path(optarg);

            if (c == 'P')
               bsv_movie_ctl(BSV_MOVIE_CTL_SET_START_PLAYBACK, NULL);
            else
               bsv_movie_ctl(BSV_MOVIE_CTL_UNSET_START_PLAYBACK, NULL);

            if (c == 'R')
               bsv_movie_ctl(BSV_MOVIE_CTL_SET_START_RECORDING, NULL);
            else
               bsv_movie_ctl(BSV_MOVIE_CTL_UNSET_START_RECORDING, NULL);
            break;

         case 'M':
            if (string_is_equal(optarg, "noload-nosave"))
            {
               rarch_ctl(RARCH_CTL_SET_SRAM_LOAD_DISABLED, NULL);
               rarch_ctl(RARCH_CTL_SET_SRAM_SAVE_DISABLED, NULL);
            }
            else if (string_is_equal(optarg, "noload-save"))
               rarch_ctl(RARCH_CTL_SET_SRAM_LOAD_DISABLED, NULL);
            else if (string_is_equal(optarg, "load-nosave"))
               rarch_ctl(RARCH_CTL_SET_SRAM_SAVE_DISABLED, NULL);
            else if (!string_is_equal(optarg, "load-save"))
            {
               RARCH_ERR("Invalid argument in --sram-mode.\n");
               retroarch_print_help(argv[0]);
               retroarch_fail(1, "retroarch_parse_input()");
            }
            break;

#ifdef HAVE_NETWORKING
         case 'H':
            retroarch_override_setting_set(
                  RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
            netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);
            break;

         case 'C':
            {
               settings_t *settings  = config_get_ptr();
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS, NULL);
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);
               strlcpy(settings->netplay.server, optarg,
                     sizeof(settings->netplay.server));
            }
            break;

         case RA_OPT_STATELESS:
            {
               settings_t *settings  = config_get_ptr();
               settings->netplay.stateless_mode = true;
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE, NULL);
            }
            break;

         case RA_OPT_CHECK_FRAMES:
            {
               settings_t *settings  = config_get_ptr();
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES, NULL);
               settings->netplay.check_frames = strtoul(optarg, NULL, 0);
            }
            break;

         case RA_OPT_PORT:
            {
               settings_t *settings  = config_get_ptr();
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT, NULL);
               settings->netplay.port = strtoul(optarg, NULL, 0);
            }
            break;

#if defined(HAVE_NETWORK_CMD)
         case RA_OPT_COMMAND:
#ifdef HAVE_COMMAND
            if (command_network_send((const char*)optarg))
               exit(0);
            else
               retroarch_fail(1, "network_cmd_send()");
#endif
            break;
#endif

#endif

         case RA_OPT_BPS:
            strlcpy(global->name.bps, optarg,
                  sizeof(global->name.bps));
            rarch_ctl(RARCH_CTL_SET_BPS_PREF, NULL);
            retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_BPS_PREF, NULL);
            break;

         case 'U':
            strlcpy(global->name.ups, optarg,
                  sizeof(global->name.ups));
            rarch_ctl(RARCH_CTL_SET_UPS_PREF, NULL);
            retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_UPS_PREF, NULL);
            break;

         case RA_OPT_IPS:
            strlcpy(global->name.ips, optarg,
                  sizeof(global->name.ips));
            rarch_ctl(RARCH_CTL_SET_IPS_PREF, NULL);
            retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_IPS_PREF, NULL);
            break;

         case RA_OPT_NO_PATCH:
            rarch_ctl(RARCH_CTL_SET_PATCH_BLOCKED, NULL);
            break;

         case 'D':
            frontend_driver_detach_console();
            break;

         case RA_OPT_MENU:
            explicit_menu = true;
            break;

         case RA_OPT_NICK:
            {
               settings_t *settings  = config_get_ptr();
               rarch_ctl(RARCH_CTL_USERNAME_SET, NULL);
               strlcpy(settings->username, optarg,
                     sizeof(settings->username));
            }
            break;

         case RA_OPT_APPENDCONFIG:
            path_set(RARCH_PATH_CONFIG_APPEND, optarg);
            break;

         case RA_OPT_SIZE:
            if (sscanf(optarg, "%ux%u",
                     recording_driver_get_width(),
                     recording_driver_get_height()) != 2)
            {
               RARCH_ERR("Wrong format for --size.\n");
               retroarch_print_help(argv[0]);
               retroarch_fail(1, "retroarch_parse_input()");
            }
            break;

         case RA_OPT_RECORDCONFIG:
            strlcpy(global->record.config, optarg,
                  sizeof(global->record.config));
            break;

         case RA_OPT_MAX_FRAMES:
            {
               unsigned max_frames = strtoul(optarg, NULL, 10);
               runloop_ctl(RUNLOOP_CTL_SET_MAX_FRAMES, &max_frames);
            }
            break;

         case RA_OPT_SUBSYSTEM:
            path_set(RARCH_PATH_SUBSYSTEM, optarg);
            break;

         case RA_OPT_FEATURES:
            retroarch_print_features();
            exit(0);

         case RA_OPT_EOF_EXIT:
            bsv_movie_ctl(BSV_MOVIE_CTL_SET_END_EOF, NULL);
            break;

         case RA_OPT_VERSION:
            retroarch_print_version();
            exit(0);

#ifdef HAVE_FILE_LOGGER
         case RA_OPT_LOG_FILE:
            retro_main_log_file_init(optarg);
            break;
#endif

         case '?':
            retroarch_print_help(argv[0]);
            retroarch_fail(1, "retroarch_parse_input()");

         default:
            RARCH_ERR("%s\n", msg_hash_to_str(MSG_ERROR_PARSING_ARGUMENTS));
            retroarch_fail(1, "retroarch_parse_input()");
      }
   }

   if (explicit_menu)
   {
      if (optind < argc)
      {
         RARCH_ERR("--menu was used, but content file was passed as well.\n");
         retroarch_fail(1, "retroarch_parse_input()");
      }
#ifdef HAVE_DYNAMIC
      else
      {
         /* Allow stray -L arguments to go through to workaround cases 
          * where it's used as "config file".
          *
          * This seems to still be the case for Android, which 
          * should be properly fixed. */
         retroarch_set_current_core_type(CORE_TYPE_DUMMY, false);
      }
#endif
   }

   if (path_is_empty(RARCH_PATH_SUBSYSTEM) && optind < argc)
   {
      /* We requested explicit ROM, so use PLAIN core type. */
      retroarch_set_current_core_type(CORE_TYPE_PLAIN, false);
      path_set(RARCH_PATH_NAMES, (const char*)argv[optind]);
   }
   else if (!path_is_empty(RARCH_PATH_SUBSYSTEM) && optind < argc)
   {
      /* We requested explicit ROM, so use PLAIN core type. */
      retroarch_set_current_core_type(CORE_TYPE_PLAIN, false);
      path_set_special(argv + optind, argc - optind);
   }
   else
      content_set_does_not_need_content();

   /* Copy SRM/state dirs used, so they can be reused on reentrancy. */
   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL) &&
         path_is_directory(global->name.savefile))
      dir_set(RARCH_DIR_SAVEFILE, global->name.savefile);

   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL) &&
         path_is_directory(global->name.savestate))
      dir_set(RARCH_DIR_SAVESTATE, global->name.savestate);
}

static bool retroarch_init_state(void)
{
   video_driver_set_active();
   audio_driver_set_active();

   rarch_ctl(RARCH_CTL_UNSET_FORCE_FULLSCREEN, NULL);

   return true;
}

bool retroarch_validate_game_options(char *s, size_t len, bool mkdir)
{
   char core_path[PATH_MAX_LENGTH];
   char config_directory[PATH_MAX_LENGTH];
   const char *core_name                  = NULL;
   const char *game_name                  = NULL;
   rarch_system_info_t *system            = NULL;

   config_directory[0] = core_path[0]     = '\0';

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   if (system)
      core_name = system->info.library_name;

   game_name = path_basename(path_get(RARCH_PATH_BASENAME));

   if (string_is_empty(core_name) || string_is_empty(game_name))
      return false;

   fill_pathname_application_special(config_directory,
         sizeof(config_directory),
         APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   /* Concatenate strings into full paths for game_path */
   fill_pathname_join_special_ext(s,
         config_directory, core_name, game_name,
         file_path_str(FILE_PATH_OPT_EXTENSION),
         len);

   fill_pathname_join(core_path,
         config_directory, core_name, sizeof(core_path));

   if (!path_is_directory(core_path) && mkdir)
      path_mkdir(core_path);

   return true;
}

/* Validates CPU features for given processor architecture.
 * Make sure we haven't compiled for something we cannot run.
 * Ideally, code would get swapped out depending on CPU support,
 * but this will do for now. */
static void retroarch_validate_cpu_features(void)
{
   uint64_t cpu = cpu_features_get();
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

static void retroarch_main_init_media(void)
{
   settings_t *settings    = config_get_ptr();
   const char    *fullpath = path_get(RARCH_PATH_CONTENT);

   if (!settings)
      return;

   if (  !settings->multimedia.builtin_mediaplayer_enable &&
         !settings->multimedia.builtin_imageviewer_enable
      )
      return;

   if (string_is_empty(fullpath))
      return;

   switch (path_is_media_type(fullpath))
   {
      case RARCH_CONTENT_MOVIE:
      case RARCH_CONTENT_MUSIC:
         if (settings->multimedia.builtin_mediaplayer_enable)
         {
#ifdef HAVE_FFMPEG
            retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
            retroarch_set_current_core_type(CORE_TYPE_FFMPEG, false);
#endif
         }
         break;
#ifdef HAVE_IMAGEVIEWER
      case RARCH_CONTENT_IMAGE:
         if (settings->multimedia.builtin_imageviewer_enable)
         {
            retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
            retroarch_set_current_core_type(CORE_TYPE_IMAGEVIEWER, false);
         }
         break;
#endif
      default:
         break;
   }
}

/**
 * retroarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Initializes the program.
 *
 * Returns: 0 on success, otherwise 1 if there was an error.
 **/
bool retroarch_main_init(int argc, char *argv[])
{
   bool init_failed = false;

   retroarch_init_state();

   if (setjmp(error_sjlj_context) > 0)
   {
      RARCH_ERR("%s: \"%s\"\n",
            msg_hash_to_str(MSG_FATAL_ERROR_RECEIVED_IN), error_string);
      return false;
   }

   rarch_ctl(RARCH_CTL_SET_ERROR_ON_INIT, NULL);
   retro_main_log_file_init(NULL);
   retroarch_parse_input(argc, argv);

   if (verbosity_is_enabled())
   {
      char str[255];

      str[0] = '\0';

      RARCH_LOG_OUTPUT("=== Build =======================================\n");
      retroarch_get_capabilities(RARCH_CAPABILITIES_CPU, str, sizeof(str));
      fprintf(stderr, "%s: %s\n", msg_hash_to_str(MSG_CAPABILITIES), str);
      fprintf(stderr, "Built: %s\n", __DATE__);
      RARCH_LOG_OUTPUT("Version: %s\n", PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
      RARCH_LOG_OUTPUT("Git: %s\n", retroarch_git_version);
#endif
      RARCH_LOG_OUTPUT("=================================================\n");
   }

   retroarch_validate_cpu_features();
   config_load();

   runloop_ctl(RUNLOOP_CTL_TASK_INIT, NULL);

   retroarch_main_init_media();

   driver_ctl(RARCH_DRIVER_CTL_INIT_PRE, NULL);

   /* Attempt to initialize core */
   if (has_set_core)
   {
      has_set_core = false;
      if (!command_event(CMD_EVENT_CORE_INIT, &explicit_current_core_type))
         init_failed = true;
   }
   else if (!command_event(CMD_EVENT_CORE_INIT, &current_core_type))
      init_failed = true;

   /* Handle core initialization failure */
   if (init_failed)
   {
#ifdef HAVE_MENU
      /* Check if menu was active prior to core initialization */
      if (menu_driver_is_alive())
      {
         /* Attempt initializing dummy core */
         current_core_type = CORE_TYPE_DUMMY;
         if (!command_event(CMD_EVENT_CORE_INIT, &current_core_type))
            goto error;
      }
      else
#endif
      {
         /* Fall back to regular error handling */
         goto error;
      }
   }

   drivers_init(DRIVERS_CMD_ALL);
   command_event(CMD_EVENT_COMMAND_INIT, NULL);
   command_event(CMD_EVENT_REMOTE_INIT, NULL);
   command_event(CMD_EVENT_REWIND_INIT, NULL);
   command_event(CMD_EVENT_CONTROLLERS_INIT, NULL);
   command_event(CMD_EVENT_RECORD_INIT, NULL);
   command_event(CMD_EVENT_CHEATS_INIT, NULL);

   path_init_savefile();

   command_event(CMD_EVENT_SET_PER_GAME_RESOLUTION, NULL);

   rarch_ctl(RARCH_CTL_UNSET_ERROR_ON_INIT, NULL);
   rarch_ctl(RARCH_CTL_SET_INITED, NULL);

   return true;

error:
   command_event(CMD_EVENT_CORE_DEINIT, NULL);
   rarch_ctl(RARCH_CTL_UNSET_INITED, NULL);
   return false;
}


bool rarch_ctl(enum rarch_ctl_state state, void *data)
{
   static bool rarch_is_sram_load_disabled = false;
   static bool rarch_is_sram_save_disabled = false;
   static bool rarch_use_sram              = false;
   static bool rarch_ups_pref              = false;
   static bool rarch_bps_pref              = false;
   static bool rarch_ips_pref              = false;
   static bool rarch_patch_blocked         = false;
#ifdef HAVE_THREAD_STORAGE
   static sthread_tls_t rarch_tls;
   const void *MAGIC_POINTER = (void*)0xB16B00B5;
#endif

   switch(state)
   {
      case RARCH_CTL_IS_PATCH_BLOCKED:
         return rarch_patch_blocked;
      case RARCH_CTL_SET_PATCH_BLOCKED:
         rarch_patch_blocked = true;
         break;
      case RARCH_CTL_UNSET_PATCH_BLOCKED:
         rarch_patch_blocked = false;
         break;
      case RARCH_CTL_IS_BPS_PREF:
         return rarch_bps_pref;
      case RARCH_CTL_SET_BPS_PREF:
         rarch_bps_pref = true;
         break;
      case RARCH_CTL_UNSET_BPS_PREF:
         rarch_bps_pref = false;
         break;
      case RARCH_CTL_IS_UPS_PREF:
         return rarch_ups_pref;
      case RARCH_CTL_SET_UPS_PREF:
         rarch_ups_pref = true;
         break;
      case RARCH_CTL_UNSET_UPS_PREF:
         rarch_ups_pref = false;
         break;
      case RARCH_CTL_IS_IPS_PREF:
         return rarch_ips_pref;
      case RARCH_CTL_SET_IPS_PREF:
         rarch_ips_pref = true;
         break;
      case RARCH_CTL_UNSET_IPS_PREF:
         rarch_ips_pref = false;
         break;
      case RARCH_CTL_IS_PLAIN_CORE:
         return (current_core_type == CORE_TYPE_PLAIN);
      case RARCH_CTL_IS_DUMMY_CORE:
         return (current_core_type == CORE_TYPE_DUMMY);
      case RARCH_CTL_USERNAME_SET:
         has_set_username = true;
         break;
      case RARCH_CTL_USERNAME_UNSET:
         has_set_username = false;
         break;
      case RARCH_CTL_HAS_SET_USERNAME:
         return has_set_username;
      case RARCH_CTL_IS_INITED:
         return rarch_is_inited;
      case RARCH_CTL_UNSET_INITED:
         rarch_is_inited         = false;
         break;
      case RARCH_CTL_SET_INITED:
         rarch_is_inited         = true;
         break;
      case RARCH_CTL_DESTROY:
         has_set_username        = false;
         rarch_is_inited         = false;
         rarch_error_on_init     = false;
         rarch_block_config_read = false;
         rarch_force_fullscreen  = false;

         runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_DEINIT, NULL);
         driver_ctl(RARCH_DRIVER_CTL_UNINIT_ALL, NULL);
         command_event(CMD_EVENT_LOG_FILE_DEINIT, NULL);

         runloop_ctl(RUNLOOP_CTL_STATE_FREE,  NULL);
         runloop_ctl(RUNLOOP_CTL_GLOBAL_FREE, NULL);
         runloop_ctl(RUNLOOP_CTL_DATA_DEINIT, NULL);
         config_free();
         break;
      case RARCH_CTL_DEINIT:
         if (!rarch_ctl(RARCH_CTL_IS_INITED, NULL))
            return false;

         driver_ctl(RARCH_DRIVER_CTL_UNINIT_ALL, NULL);
         break;
      case RARCH_CTL_PREINIT:

         command_event(CMD_EVENT_HISTORY_DEINIT, NULL);

         config_init();

         runloop_ctl(RUNLOOP_CTL_CLEAR_STATE, NULL);
         break;
      case RARCH_CTL_MAIN_DEINIT:
         if (!rarch_ctl(RARCH_CTL_IS_INITED, NULL))
            return false;
         command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
         command_event(CMD_EVENT_COMMAND_DEINIT, NULL);
         command_event(CMD_EVENT_REMOTE_DEINIT, NULL);

         command_event(CMD_EVENT_AUTOSAVE_DEINIT, NULL);

         command_event(CMD_EVENT_RECORD_DEINIT, NULL);

         event_save_files();

         command_event(CMD_EVENT_REWIND_DEINIT, NULL);
         command_event(CMD_EVENT_CHEATS_DEINIT, NULL);
         command_event(CMD_EVENT_BSV_MOVIE_DEINIT, NULL);

         command_event(CMD_EVENT_CORE_DEINIT, NULL);

         command_event(CMD_EVENT_TEMPORARY_CONTENT_DEINIT, NULL);

         path_deinit_subsystem();
         path_deinit_savefile();

         rarch_ctl(RARCH_CTL_UNSET_INITED, NULL);

#ifdef HAVE_THREAD_STORAGE
         sthread_tls_delete(&rarch_tls);
#endif
         break;
      case RARCH_CTL_INIT:
         rarch_ctl(RARCH_CTL_DEINIT, NULL);
#ifdef HAVE_THREAD_STORAGE
         sthread_tls_create(&rarch_tls);
         sthread_tls_set(&rarch_tls, MAGIC_POINTER);
#endif
         retroarch_init_state();
         {
            unsigned i;
            for (i = 0; i < MAX_USERS; i++)
               input_config_set_device(i, RETRO_DEVICE_JOYPAD);
         }
         runloop_ctl(RUNLOOP_CTL_HTTPSERVER_INIT, NULL);
         runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_INIT, NULL);
         break;
      case RARCH_CTL_SET_PATHS_REDIRECT:
         if (content_does_not_need_content())
            return false;
         path_set_redirect();
         break;
      case RARCH_CTL_IS_SRAM_LOAD_DISABLED:
         return rarch_is_sram_load_disabled;
      case RARCH_CTL_SET_SRAM_LOAD_DISABLED:
         rarch_is_sram_load_disabled = true;
         break;
      case RARCH_CTL_UNSET_SRAM_LOAD_DISABLED:
         rarch_is_sram_load_disabled = false;
         break;
      case RARCH_CTL_IS_SRAM_SAVE_DISABLED:
         return rarch_is_sram_save_disabled;
      case RARCH_CTL_SET_SRAM_SAVE_DISABLED:
         rarch_is_sram_save_disabled = true;
         break;
      case RARCH_CTL_UNSET_SRAM_SAVE_DISABLED:
         rarch_is_sram_save_disabled = false;
         break;
      case RARCH_CTL_IS_SRAM_USED:
         return rarch_use_sram;
      case RARCH_CTL_SET_SRAM_ENABLE:
         rarch_use_sram = rarch_ctl(RARCH_CTL_IS_PLAIN_CORE, NULL)
            && !content_does_not_need_content();
         break;
      case RARCH_CTL_SET_SRAM_ENABLE_FORCE:
         rarch_use_sram = true;
         break;
      case RARCH_CTL_UNSET_SRAM_ENABLE:
         rarch_use_sram = false;
         break;
      case RARCH_CTL_SET_ERROR_ON_INIT:
         rarch_error_on_init = true;
         break;
      case RARCH_CTL_UNSET_ERROR_ON_INIT:
         rarch_error_on_init = false;
         break;
      case RARCH_CTL_IS_ERROR_ON_INIT:
         return rarch_error_on_init;
      case RARCH_CTL_SET_FORCE_FULLSCREEN:
         rarch_force_fullscreen = true;
         break;
      case RARCH_CTL_UNSET_FORCE_FULLSCREEN:
         rarch_force_fullscreen = false;
         break;
      case RARCH_CTL_IS_FORCE_FULLSCREEN:
         return rarch_force_fullscreen;
      case RARCH_CTL_SET_BLOCK_CONFIG_READ:
         rarch_block_config_read = true;
         break;
      case RARCH_CTL_UNSET_BLOCK_CONFIG_READ:
         rarch_block_config_read = false;
         break;
      case RARCH_CTL_IS_BLOCK_CONFIG_READ:
         return rarch_block_config_read;
      case RARCH_CTL_MENU_RUNNING:
#ifdef HAVE_MENU
         menu_driver_ctl(RARCH_MENU_CTL_SET_TOGGLE, NULL);
#endif
#ifdef HAVE_OVERLAY
         {
            settings_t *settings                    = config_get_ptr();
            if (settings->input.overlay_hide_in_menu)
               command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
         }
#endif
         break;
      case RARCH_CTL_MENU_RUNNING_FINISHED:
#ifdef HAVE_MENU
         menu_driver_ctl(RARCH_MENU_CTL_UNSET_TOGGLE, NULL);
#endif
         video_driver_set_texture_enable(false, false);
#ifdef HAVE_OVERLAY
         {
            settings_t *settings                    = config_get_ptr();
            if (settings && settings->input.overlay_hide_in_menu)
               command_event(CMD_EVENT_OVERLAY_INIT, NULL);
         }
#endif
         break;
      case RARCH_CTL_IS_MAIN_THREAD:
#ifdef HAVE_THREAD_STORAGE
         return sthread_tls_get(&rarch_tls) == MAGIC_POINTER;
#else
         return true;
#endif
      case RARCH_CTL_NONE:
      default:
         return false;
   }

   return true;
}


bool retroarch_override_setting_is_set(enum rarch_override_setting enum_idx, void *data)
{
   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               return BIT128_GET(has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         return has_set_verbosity;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         return has_set_libretro;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         return has_set_libretro_directory;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         return has_set_save_path;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         return has_set_state_path;
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         return has_set_netplay_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         return has_set_netplay_ip_address;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         return has_set_netplay_ip_port;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         return has_set_netplay_stateless_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         return has_set_netplay_check_frames;
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         return has_set_ups_pref;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         return has_set_bps_pref;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         return has_set_ips_pref;

      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }

   return false;
}

void retroarch_override_setting_set(enum rarch_override_setting enum_idx, void *data)
{
   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               BIT128_SET(has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         has_set_verbosity = true;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         has_set_libretro = true;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         has_set_libretro_directory = true;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         has_set_save_path = true;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         has_set_state_path = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         has_set_netplay_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         has_set_netplay_ip_address = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         has_set_netplay_ip_port = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         has_set_netplay_stateless_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         has_set_netplay_check_frames = true;
         break;
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         has_set_ups_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         has_set_bps_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         has_set_ips_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }
}

void retroarch_override_setting_unset(enum rarch_override_setting enum_idx, void *data)
{
   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               BIT128_CLEAR(has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         has_set_verbosity = false;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         has_set_libretro = false;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         has_set_libretro_directory = false;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         has_set_save_path = false;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         has_set_state_path = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         has_set_netplay_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         has_set_netplay_ip_address = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         has_set_netplay_ip_port = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         has_set_netplay_stateless_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         has_set_netplay_check_frames = false;
         break;
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         has_set_ups_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         has_set_bps_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         has_set_ips_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }
}

void retroarch_override_setting_free_state(void)
{
   unsigned i;
   for (i = 0; i < RARCH_OVERRIDE_SETTING_LAST; i++)
   {
      if (i == RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE)
      {
         unsigned j;
         for (j = 0; j < MAX_USERS; j++)
            retroarch_override_setting_unset((enum rarch_override_setting)(i), &j);
      }
      else
         retroarch_override_setting_unset((enum rarch_override_setting)(i), NULL);
   }
}

int retroarch_get_capabilities(enum rarch_capabilities type,
      char *s, size_t len)
{
   switch (type)
   {
      case RARCH_CAPABILITIES_CPU:
         {
            uint64_t cpu = cpu_features_get();

            if (cpu & RETRO_SIMD_MMX)
               strlcat(s, "MMX ", len);
            if (cpu & RETRO_SIMD_MMXEXT)
               strlcat(s, "MMXEXT ", len);
            if (cpu & RETRO_SIMD_SSE)
               strlcat(s, "SSE1 ", len);
            if (cpu & RETRO_SIMD_SSE2)
               strlcat(s, "SSE2 ", len);
            if (cpu & RETRO_SIMD_SSE3)
               strlcat(s, "SSE3 ", len);
            if (cpu & RETRO_SIMD_SSSE3)
               strlcat(s, "SSSE3 ", len);
            if (cpu & RETRO_SIMD_SSE4)
               strlcat(s, "SSE4 ", len);
            if (cpu & RETRO_SIMD_SSE42)
               strlcat(s, "SSE4.2 ", len);
            if (cpu & RETRO_SIMD_AVX)
               strlcat(s, "AVX ", len);
            if (cpu & RETRO_SIMD_AVX2)
               strlcat(s, "AVX2 ", len);
            if (cpu & RETRO_SIMD_VFPU)
               strlcat(s, "VFPU ", len);
            if (cpu & RETRO_SIMD_NEON)
               strlcat(s, "NEON ", len);
            if (cpu & RETRO_SIMD_VFPV3)
               strlcat(s, "VFPv3 ", len);
            if (cpu & RETRO_SIMD_VFPV4)
               strlcat(s, "VFPv4 ", len);
            if (cpu & RETRO_SIMD_PS)
               strlcat(s, "PS ", len);
            if (cpu & RETRO_SIMD_AES)
               strlcat(s, "AES ", len);
            if (cpu & RETRO_SIMD_VMX)
               strlcat(s, "VMX ", len);
            if (cpu & RETRO_SIMD_VMX128)
               strlcat(s, "VMX128 ", len);
            if (cpu & RETRO_SIMD_ASIMD)
               strlcat(s, "ASIMD ", len);
         }
         break;
      case RARCH_CAPABILITIES_COMPILER:
#if defined(_MSC_VER)
         snprintf(s, len, "%s: MSVC (%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               _MSC_VER, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#elif defined(__SNC__)
         snprintf(s, len, "%s: SNC (%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __SN_VER__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(_WIN32) && defined(__GNUC__)
         snprintf(s, len, "%s: MinGW (%d.%d.%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#elif defined(__clang__)
         snprintf(s, len, "%s: Clang/LLVM (%s) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __clang_version__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(__GNUC__)
         snprintf(s, len, "%s: GCC (%d.%d.%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#else
         snprintf(s, len, "%s %u-bit",
               msg_hash_to_str(MSG_UNKNOWN_COMPILER),
               (unsigned)(CHAR_BIT * sizeof(size_t)));
#endif
         break;
      default:
      case RARCH_CAPABILITIES_NONE:
         break;
   }

   return 0;
}

void retroarch_set_current_core_type(enum rarch_core_type type, bool explicitly_set)
{
   if (explicitly_set && !has_set_core)
   {
      has_set_core                = true;
      explicit_current_core_type  = type;
      current_core_type           = type;
   }
   else if (!has_set_core)
      current_core_type          = type;
}

/**
 * retroarch_fail:
 * @error_code  : Error code.
 * @error       : Error message to show.
 *
 * Sanely kills the program.
 **/
void retroarch_fail(int error_code, const char *error)
{
   /* We cannot longjmp unless we're in retroarch_main_init().
    * If not, something went very wrong, and we should
    * just exit right away. */
   retro_assert(rarch_ctl(RARCH_CTL_IS_ERROR_ON_INIT, NULL));

   strlcpy(error_string, error, sizeof(error_string));
   longjmp(error_sjlj_context, error_code);
}

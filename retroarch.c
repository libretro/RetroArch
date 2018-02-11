/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2015-2017 - Andrés Suárez
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
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <math.h>

#include <boolean.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <retro_timers.h>

#include <compat/strl.h>
#include <compat/getopt.h>
#include <audio/audio_mixer.h>
#include <compat/posix_string.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <queues/message_queue.h>
#include <queues/task_queue.h>
#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_COMMAND
#include "command.h"
#endif

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#include "menu/menu_event.h"
#include "menu/widgets/menu_dialog.h"
#include "menu/widgets/menu_input_dialog.h"
#endif

#ifdef HAVE_CHEEVOS
#include "cheevos/cheevos.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#endif

#if defined(HAVE_HTTPSERVER) && defined(HAVE_ZLIB)
#include "network/httpserver/httpserver.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "autosave.h"
#include "config.features.h"
#include "content.h"
#include "core_type.h"
#include "core_info.h"
#include "dynamic.h"
#include "driver.h"
#include "input/input_driver.h"
#include "msg_hash.h"
#include "movie.h"
#include "dirs.h"
#include "paths.h"
#include "file_path_special.h"
#include "ui/ui_companion_driver.h"
#include "verbosity.h"

#include "frontend/frontend_driver.h"
#include "audio/audio_driver.h"
#include "camera/camera_driver.h"
#include "record/record_driver.h"
#include "core.h"
#include "configuration.h"
#include "list_special.h"
#include "managers/core_option_manager.h"
#include "managers/cheat_manager.h"
#include "managers/state_manager.h"
#include "tasks/tasks_internal.h"
#include "performance_counters.h"

#include "version.h"
#include "version_git.h"

#include "retroarch.h"

#include "command.h"

#define _PSUPP(var, name, desc) printf("  %s:\n\t\t%s: %s\n", name, desc, _##var##_supp ? "yes" : "no")

#define FAIL_CPU(simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   retroarch_fail(1, "validate_cpu_features()"); \
} while(0)

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

#define SHADER_FILE_WATCH_DELAY_MSEC 500

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

enum  runloop_state
{
   RUNLOOP_STATE_ITERATE = 0,
   RUNLOOP_STATE_POLLED_AND_SLEEP,
   RUNLOOP_STATE_SLEEP,
   RUNLOOP_STATE_MENU_ITERATE,
   RUNLOOP_STATE_END,
   RUNLOOP_STATE_QUIT
};

typedef struct runloop_ctx_msg_info
{
   const char *msg;
   unsigned prio;
   unsigned duration;
   bool flush;
} runloop_ctx_msg_info_t;

static jmp_buf error_sjlj_context;
static enum rarch_core_type current_core_type           = CORE_TYPE_PLAIN;
static enum rarch_core_type explicit_current_core_type  = CORE_TYPE_PLAIN;
static char error_string[255]                           = {0};
static char runtime_shader_preset[255]                          = {0};

#ifdef HAVE_THREAD_STORAGE
static sthread_tls_t rarch_tls;
const void *MAGIC_POINTER                               = (void*)(uintptr_t)0xB16B00B5;
#endif

static retro_bits_t has_set_libretro_device;

static bool has_set_core                                   = false;
static bool has_set_username                               = false;
static bool rarch_is_inited                                = false;
static bool rarch_error_on_init                            = false;
static bool rarch_block_config_read                        = false;
static bool rarch_force_fullscreen                         = false;
static bool has_set_verbosity                              = false;
static bool has_set_libretro                               = false;
static bool has_set_libretro_directory                     = false;
static bool has_set_save_path                              = false;
static bool has_set_state_path                             = false;
static bool has_set_netplay_mode                           = false;
static bool has_set_netplay_ip_address                     = false;
static bool has_set_netplay_ip_port                        = false;
static bool has_set_netplay_stateless_mode                 = false;
static bool has_set_netplay_check_frames                   = false;
static bool has_set_ups_pref                               = false;
static bool has_set_bps_pref                               = false;
static bool has_set_ips_pref                               = false;

static bool rarch_is_sram_load_disabled                    = false;
static bool rarch_is_sram_save_disabled                    = false;
static bool rarch_use_sram                                 = false;
static bool rarch_ups_pref                                 = false;
static bool rarch_bps_pref                                 = false;
static bool rarch_ips_pref                                 = false;
static bool rarch_patch_blocked                            = false;

static bool runloop_force_nonblock                         = false;
static bool runloop_paused                                 = false;
static bool runloop_idle                                   = false;
static bool runloop_exec                                   = false;
static bool runloop_slowmotion                             = false;
static bool runloop_fastmotion                             = false;
static bool runloop_shutdown_initiated                     = false;
static bool runloop_core_shutdown_initiated                = false;
static bool runloop_perfcnt_enable                         = false;
static bool runloop_overrides_active                       = false;
static bool runloop_remaps_core_active                     = false;
static bool runloop_remaps_game_active                     = false;
static bool runloop_game_options_active                    = false;
static bool runloop_missing_bios                           = false;
static bool runloop_autosave                               = false;

static rarch_system_info_t runloop_system;
static struct retro_frame_time_callback runloop_frame_time;
static retro_keyboard_event_t runloop_key_event            = NULL;
static retro_keyboard_event_t runloop_frontend_key_event   = NULL;
static core_option_manager_t *runloop_core_options         = NULL;
#ifdef HAVE_THREADS
static slock_t *_runloop_msg_queue_lock                    = NULL;
#endif
static msg_queue_t *runloop_msg_queue                      = NULL;

static unsigned runloop_pending_windowed_scale             = 0;
static unsigned runloop_max_frames                         = 0;

static retro_usec_t runloop_frame_time_last                = 0;
static retro_time_t frame_limit_minimum_time               = 0.0;
static retro_time_t frame_limit_last_time                  = 0.0;

extern bool input_driver_flushing_input;

#ifdef HAVE_THREADS
void runloop_msg_queue_lock(void)
{
   slock_lock(_runloop_msg_queue_lock);
}

void runloop_msg_queue_unlock(void)
{
   slock_unlock(_runloop_msg_queue_lock);
}
#endif

static void retroarch_msg_queue_deinit(void)
{
#ifdef HAVE_THREADS
   runloop_msg_queue_lock();
#endif

   if (!runloop_msg_queue)
      return;

   msg_queue_free(runloop_msg_queue);

#ifdef HAVE_THREADS
   runloop_msg_queue_unlock();
   slock_free(_runloop_msg_queue_lock);
   _runloop_msg_queue_lock = NULL;
#endif

   runloop_msg_queue = NULL;
}

static void retroarch_msg_queue_init(void)
{
   retroarch_msg_queue_deinit();
   runloop_msg_queue = msg_queue_new(8);

#ifdef HAVE_THREADS
   _runloop_msg_queue_lock = slock_new();
#endif
}

static void retroarch_override_setting_free_state(void)
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


static void global_free(void)
{
   global_t *global = NULL;

   content_deinit();

   path_deinit_subsystem();
   command_event(CMD_EVENT_RECORD_DEINIT, NULL);
   command_event(CMD_EVENT_LOG_FILE_DEINIT, NULL);

   rarch_block_config_read               = false;
   rarch_is_sram_load_disabled           = false;
   rarch_is_sram_save_disabled           = false;
   rarch_use_sram                        = false;
   rarch_bps_pref                        = false;
   rarch_ips_pref                        = false;
   rarch_ups_pref                        = false;
   rarch_patch_blocked                   = false;
   runloop_overrides_active              = false;
   runloop_remaps_core_active            = false;
   runloop_remaps_game_active            = false;

   core_unset_input_descriptors();

   global = global_get_ptr();
   path_clear_all();
   dir_clear_all();
   if (global)
   {
      if (!string_is_empty(global->name.remapfile))
         free(global->name.remapfile);
   }
   memset(global, 0, sizeof(struct global));
   retroarch_override_setting_free_state();
}

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
   _PSUPP(wasapi,          "WASAPI",     "Audio driver");
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
   puts("      --log-file FILE   Log messages to FILE.");
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

   has_set_username                      = false;
   rarch_ups_pref                        = false;
   rarch_ips_pref                        = false;
   rarch_bps_pref                        = false;
   *global->name.ups                     = '\0';
   *global->name.bps                     = '\0';
   *global->name.ips                     = '\0';

   rarch_ctl(RARCH_CTL_UNSET_OVERRIDES_ACTIVE, NULL);

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
               unsigned id              = 0;
               struct string_list *list = string_split(optarg, ":");
               int    port              = 0;

               if (list && list->size == 2)
               {
                  port = (int)strtol(list->elems[0].data, NULL, 0);
                  id   = (unsigned)strtoul(list->elems[1].data, NULL, 0);
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
            }
            break;

         case 'A':
            {
               unsigned new_port;
               int port = (int)strtol(optarg, NULL, 0);

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
            rarch_force_fullscreen = true;
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
               int port = (int)strtol(optarg, NULL, 0);

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
            RARCH_LOG("Set config file to : %s\n", optarg);
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
               strlcpy(settings->paths.directory_libretro, optarg,
                     sizeof(settings->paths.directory_libretro));

               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY, NULL);
               RARCH_WARN("Using old --libretro behavior. "
                     "Setting libretro_directory to \"%s\" instead.\n",
                     optarg);
            }
            else if (filestream_exists(optarg))
            {
               rarch_ctl(RARCH_CTL_SET_LIBRETRO_PATH, optarg);
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
               rarch_is_sram_load_disabled = true;
               rarch_is_sram_save_disabled = true;
            }
            else if (string_is_equal(optarg, "noload-save"))
               rarch_is_sram_load_disabled = true;
            else if (string_is_equal(optarg, "load-nosave"))
               rarch_is_sram_save_disabled = true;
            else if (string_is_not_equal(optarg, "load-save"))
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
               strlcpy(settings->paths.netplay_server, optarg,
                     sizeof(settings->paths.netplay_server));
            }
            break;

         case RA_OPT_STATELESS:
            {
               settings_t *settings  = config_get_ptr();

               configuration_set_bool(settings,
                     settings->bools.netplay_stateless_mode, true);

               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE, NULL);
            }
            break;

         case RA_OPT_CHECK_FRAMES:
            {
               settings_t *settings  = config_get_ptr();
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES, NULL);

               configuration_set_int(settings,
                     settings->ints.netplay_check_frames,
                     (int)strtoul(optarg, NULL, 0));
            }
            break;

         case RA_OPT_PORT:
            {
               settings_t *settings  = config_get_ptr();
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT, NULL);
               configuration_set_uint(settings,
                     settings->uints.netplay_port,
                     (int)strtoul(optarg, NULL, 0));
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
            rarch_bps_pref = true;
            retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_BPS_PREF, NULL);
            break;

         case 'U':
            strlcpy(global->name.ups, optarg,
                  sizeof(global->name.ups));
            rarch_ups_pref = true;
            retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_UPS_PREF, NULL);
            break;

         case RA_OPT_IPS:
            strlcpy(global->name.ips, optarg,
                  sizeof(global->name.ips));
            rarch_ips_pref = true;
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

               has_set_username = true;

               strlcpy(settings->paths.username, optarg,
                     sizeof(settings->paths.username));
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
            runloop_max_frames  = (unsigned)strtoul(optarg, NULL, 10);
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

#ifdef HAVE_GIT_VERSION
   RARCH_LOG("RetroArch %s (Git %s)\n",
         PACKAGE_VERSION, retroarch_git_version);
#endif

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

   return true;
}

bool retroarch_validate_game_options(char *s, size_t len, bool mkdir)
{
   char *core_path                        = NULL;
   char *config_directory                 = NULL;
   size_t str_size                        = PATH_MAX_LENGTH * sizeof(char);
   const char *core_name                  = runloop_system.info.library_name;
   const char *game_name                  = path_basename(path_get(RARCH_PATH_BASENAME));

   if (string_is_empty(core_name) || string_is_empty(game_name))
      return false;

   core_path                              = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   config_directory                       = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   config_directory[0] = core_path[0]     = '\0';

   fill_pathname_application_special(config_directory,
         str_size, APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   /* Concatenate strings into full paths for game_path */
   fill_pathname_join_special_ext(s,
         config_directory, core_name, game_name,
         file_path_str(FILE_PATH_OPT_EXTENSION),
         len);

   fill_pathname_join(core_path,
         config_directory, core_name, str_size);

   if (!path_is_directory(core_path) && mkdir)
      path_mkdir(core_path);

   free(core_path);
   free(config_directory);
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
   settings_t *settings     = config_get_ptr();
   const char    *fullpath  = path_get(RARCH_PATH_CONTENT);
   bool builtin_imageviewer = false;
   bool builtin_mediaplayer = false;

   if (!settings)
      return;

   builtin_imageviewer      = settings->bools.multimedia_builtin_imageviewer_enable;
   builtin_mediaplayer      = settings->bools.multimedia_builtin_mediaplayer_enable;

   if (!builtin_mediaplayer && !builtin_imageviewer)
      return;

   if (string_is_empty(fullpath))
      return;

   switch (path_is_media_type(fullpath))
   {
      case RARCH_CONTENT_MOVIE:
      case RARCH_CONTENT_MUSIC:
         if (builtin_mediaplayer)
         {
#ifdef HAVE_FFMPEG
            retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
            retroarch_set_current_core_type(CORE_TYPE_FFMPEG, false);
#endif
         }
         break;
#ifdef HAVE_IMAGEVIEWER
      case RARCH_CONTENT_IMAGE:
         if (builtin_imageviewer)
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
 * Returns: true on success, otherwise false if there was an error.
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

   rarch_error_on_init = true;

   retro_main_log_file_init(NULL);
   retroarch_parse_input(argc, argv);

   if (verbosity_is_enabled())
   {
      char str[128];

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

   rarch_ctl(RARCH_CTL_TASK_INIT, NULL);

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
   command_event(CMD_EVENT_MAPPER_INIT, NULL);
   command_event(CMD_EVENT_REWIND_INIT, NULL);
   command_event(CMD_EVENT_CONTROLLERS_INIT, NULL);
   command_event(CMD_EVENT_RECORD_INIT, NULL);
   command_event(CMD_EVENT_CHEATS_INIT, NULL);

   path_init_savefile();

   command_event(CMD_EVENT_SET_PER_GAME_RESOLUTION, NULL);

   rarch_error_on_init     = false;
   rarch_is_inited         = true;

   return true;

error:
   command_event(CMD_EVENT_CORE_DEINIT, NULL);
   rarch_is_inited         = false;
   return false;
}

bool retroarch_is_on_main_thread(void)
{
#ifdef HAVE_THREAD_STORAGE
   if (sthread_tls_get(&rarch_tls) != MAGIC_POINTER)
      return false;
#endif
   return true;
}

void rarch_menu_running(void)
{
#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_SET_TOGGLE, NULL);
   /* Prevent stray input */
   input_driver_set_flushing_input();
#endif
#ifdef HAVE_OVERLAY
   {
      settings_t *settings                    = config_get_ptr();
      if (settings && settings->bools.input_overlay_hide_in_menu)
         command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
   }
#endif
}

void rarch_menu_running_finished(void)
{
#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_UNSET_TOGGLE, NULL);
   /* Prevent stray input */
   input_driver_set_flushing_input();
#endif
   video_driver_set_texture_enable(false, false);
#ifdef HAVE_OVERLAY
   {
      settings_t *settings                    = config_get_ptr();
      if (settings && settings->bools.input_overlay_hide_in_menu)
         command_event(CMD_EVENT_OVERLAY_INIT, NULL);
   }
#endif
}

/**
 * rarch_game_specific_options:
 *
 * Returns: true (1) if a game specific core
 * options path has been found,
 * otherwise false (0).
 **/
static bool rarch_game_specific_options(char **output)
{
   char *game_path       = (char*)malloc(8192 * sizeof(char));
   size_t game_path_size = 8192 * sizeof(char);

   game_path[0] ='\0';

   if (!retroarch_validate_game_options(game_path,
            game_path_size, false))
      goto error;

   if (!config_file_exists(game_path))
      goto error;

   RARCH_LOG("%s %s\n",
         msg_hash_to_str(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT),
         game_path);
   *output = strdup(game_path);
   free(game_path);
   return true;

error:
   free(game_path);
   return false;
}

bool rarch_ctl(enum rarch_ctl_state state, void *data)
{
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
      case RARCH_CTL_UNSET_BPS_PREF:
         rarch_bps_pref = false;
         break;
      case RARCH_CTL_IS_UPS_PREF:
         return rarch_ups_pref;
      case RARCH_CTL_UNSET_UPS_PREF:
         rarch_ups_pref = false;
         break;
      case RARCH_CTL_IS_IPS_PREF:
         return rarch_ips_pref;
      case RARCH_CTL_UNSET_IPS_PREF:
         rarch_ips_pref = false;
         break;
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
      case RARCH_CTL_DESTROY:
         has_set_username        = false;
         rarch_is_inited         = false;
         rarch_error_on_init     = false;
         rarch_block_config_read = false;

         retroarch_msg_queue_deinit();
         driver_uninit(DRIVERS_CMD_ALL);
         command_event(CMD_EVENT_LOG_FILE_DEINIT, NULL);

         rarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
         global_free();
         rarch_ctl(RARCH_CTL_DATA_DEINIT, NULL);
         config_free();
         break;
      case RARCH_CTL_PREINIT:
         libretro_free_system_info(&runloop_system.info);
         command_event(CMD_EVENT_HISTORY_DEINIT, NULL);

         config_init();

         driver_ctl(RARCH_DRIVER_CTL_DEINIT,  NULL);
         rarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
         global_free();
         break;
      case RARCH_CTL_MAIN_DEINIT:
         if (!rarch_is_inited)
            return false;
         command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
         command_event(CMD_EVENT_COMMAND_DEINIT, NULL);
         command_event(CMD_EVENT_REMOTE_DEINIT, NULL);
         command_event(CMD_EVENT_MAPPER_DEINIT, NULL);

         command_event(CMD_EVENT_AUTOSAVE_DEINIT, NULL);

         command_event(CMD_EVENT_RECORD_DEINIT, NULL);

         event_save_files();

         command_event(CMD_EVENT_REWIND_DEINIT, NULL);
         command_event(CMD_EVENT_CHEATS_DEINIT, NULL);
         command_event(CMD_EVENT_BSV_MOVIE_DEINIT, NULL);

         command_event(CMD_EVENT_CORE_DEINIT, NULL);

         content_deinit();

         path_deinit_subsystem();
         path_deinit_savefile();

         rarch_is_inited         = false;

#ifdef HAVE_THREAD_STORAGE
         sthread_tls_delete(&rarch_tls);
#endif
         break;
      case RARCH_CTL_INIT:
         if (rarch_is_inited)
            driver_uninit(DRIVERS_CMD_ALL);

#ifdef HAVE_THREAD_STORAGE
         sthread_tls_create(&rarch_tls);
         sthread_tls_set(&rarch_tls, MAGIC_POINTER);
#endif
         retroarch_init_state();
         {
            uint8_t i;
            for (i = 0; i < MAX_USERS; i++)
               input_config_set_device(i, RETRO_DEVICE_JOYPAD);
         }
         rarch_ctl(RARCH_CTL_HTTPSERVER_INIT, NULL);
         retroarch_msg_queue_init();
         break;
      case RARCH_CTL_IS_SRAM_LOAD_DISABLED:
         return rarch_is_sram_load_disabled;
      case RARCH_CTL_IS_SRAM_SAVE_DISABLED:
         return rarch_is_sram_save_disabled;
      case RARCH_CTL_IS_SRAM_USED:
         return rarch_use_sram;
      case RARCH_CTL_SET_SRAM_ENABLE:
         {
            bool contentless = false;
            bool is_inited   = false;
            content_get_status(&contentless, &is_inited);
            rarch_use_sram = (current_core_type == CORE_TYPE_PLAIN)
               && !contentless;
         }
         break;
      case RARCH_CTL_SET_SRAM_ENABLE_FORCE:
         rarch_use_sram = true;
         break;
      case RARCH_CTL_UNSET_SRAM_ENABLE:
         rarch_use_sram = false;
         break;
      case RARCH_CTL_SET_BLOCK_CONFIG_READ:
         rarch_block_config_read = true;
         break;
      case RARCH_CTL_UNSET_BLOCK_CONFIG_READ:
         rarch_block_config_read = false;
         break;
      case RARCH_CTL_IS_BLOCK_CONFIG_READ:
         return rarch_block_config_read;
      case RARCH_CTL_SYSTEM_INFO_INIT:
         core_get_system_info(&runloop_system.info);

         if (!runloop_system.info.library_name)
            runloop_system.info.library_name = msg_hash_to_str(MSG_UNKNOWN);
         if (!runloop_system.info.library_version)
            runloop_system.info.library_version = "v0";

         video_driver_set_title_buf();

         strlcpy(runloop_system.valid_extensions,
               runloop_system.info.valid_extensions ?
               runloop_system.info.valid_extensions : DEFAULT_EXT,
               sizeof(runloop_system.valid_extensions));
         break;
      case RARCH_CTL_GET_CORE_OPTION_SIZE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            *idx = (unsigned)core_option_manager_size(runloop_core_options);
         }
         break;
      case RARCH_CTL_HAS_CORE_OPTIONS:
         return (runloop_core_options != NULL);
      case RARCH_CTL_CORE_OPTIONS_LIST_GET:
         {
            core_option_manager_t **coreopts = (core_option_manager_t**)data;
            if (!coreopts)
               return false;
            *coreopts = runloop_core_options;
         }
         break;
      case RARCH_CTL_SYSTEM_INFO_FREE:
         if (runloop_system.subsystem.data)
            free(runloop_system.subsystem.data);
         runloop_system.subsystem.data = NULL;
         runloop_system.subsystem.size = 0;

         if (runloop_system.ports.data)
            free(runloop_system.ports.data);
         runloop_system.ports.data = NULL;
         runloop_system.ports.size = 0;

         if (runloop_system.mmaps.descriptors)
            free((void *)runloop_system.mmaps.descriptors);
         runloop_system.mmaps.descriptors     = NULL;
         runloop_system.mmaps.num_descriptors = 0;

         runloop_key_event          = NULL;
         runloop_frontend_key_event = NULL;

         audio_driver_unset_callback();

#if 0
         if (!string_is_empty(runloop_system.info.library_name))
            free((void*)runloop_system.info.library_name);
         if (!string_is_empty(runloop_system.info.library_version))
            free((void*)runloop_system.info.library_version);
         if (!string_is_empty(runloop_system.info.valid_extensions))
            free((void*)runloop_system.info.valid_extensions);
#endif

         runloop_system.info.library_name          = NULL;
         runloop_system.info.library_version       = NULL;
         runloop_system.info.valid_extensions      = NULL;
         runloop_system.info.need_fullpath         = false;
         runloop_system.info.block_extract         = false;

         memset(&runloop_system, 0, sizeof(rarch_system_info_t));
         break;
      case RARCH_CTL_SET_FRAME_TIME_LAST:
         runloop_frame_time_last        = 0;
         break;
      case RARCH_CTL_SET_OVERRIDES_ACTIVE:
         runloop_overrides_active = true;
         break;
      case RARCH_CTL_UNSET_OVERRIDES_ACTIVE:
         runloop_overrides_active = false;
         break;
      case RARCH_CTL_IS_OVERRIDES_ACTIVE:
         return runloop_overrides_active;
      case RARCH_CTL_SET_REMAPS_CORE_ACTIVE:
         runloop_remaps_core_active = true;
         break;
      case RARCH_CTL_UNSET_REMAPS_CORE_ACTIVE:
         runloop_remaps_core_active = false;
         break;
      case RARCH_CTL_IS_REMAPS_CORE_ACTIVE:
         return runloop_remaps_core_active;
      case RARCH_CTL_SET_REMAPS_GAME_ACTIVE:
         runloop_remaps_game_active = true;
         break;
      case RARCH_CTL_UNSET_REMAPS_GAME_ACTIVE:
         runloop_remaps_game_active = false;
         break;
      case RARCH_CTL_IS_REMAPS_GAME_ACTIVE:
         return runloop_remaps_game_active;
      case RARCH_CTL_SET_MISSING_BIOS:
         runloop_missing_bios = true;
         break;
      case RARCH_CTL_UNSET_MISSING_BIOS:
         runloop_missing_bios = false;
         break;
      case RARCH_CTL_IS_MISSING_BIOS:
         return runloop_missing_bios;
      case RARCH_CTL_IS_GAME_OPTIONS_ACTIVE:
         return runloop_game_options_active;
      case RARCH_CTL_SET_FRAME_LIMIT:
         {
            settings_t *settings       = config_get_ptr();
            struct retro_system_av_info *av_info =
               video_viewport_get_system_av_info();
            float fastforward_ratio              =
               (settings->floats.fastforward_ratio == 0.0f)
               ? 1.0f : settings->floats.fastforward_ratio;

            frame_limit_last_time    = cpu_features_get_time_usec();
            frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f
                  / (av_info->timing.fps * fastforward_ratio));
         }
         break;
      case RARCH_CTL_GET_PERFCNT:
         {
            bool **perfcnt = (bool**)data;
            if (!perfcnt)
               return false;
            *perfcnt = &runloop_perfcnt_enable;
         }
         break;
      case RARCH_CTL_SET_PERFCNT_ENABLE:
         runloop_perfcnt_enable = true;
         break;
      case RARCH_CTL_UNSET_PERFCNT_ENABLE:
         runloop_perfcnt_enable = false;
         break;
      case RARCH_CTL_IS_PERFCNT_ENABLE:
         return runloop_perfcnt_enable;
      case RARCH_CTL_SET_NONBLOCK_FORCED:
         runloop_force_nonblock = true;
         break;
      case RARCH_CTL_UNSET_NONBLOCK_FORCED:
         runloop_force_nonblock = false;
         break;
      case RARCH_CTL_IS_NONBLOCK_FORCED:
         return runloop_force_nonblock;
      case RARCH_CTL_SET_FRAME_TIME:
         {
            const struct retro_frame_time_callback *info =
               (const struct retro_frame_time_callback*)data;
#ifdef HAVE_NETWORKING
            /* retro_run() will be called in very strange and
             * mysterious ways, have to disable it. */
            if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
               return false;
#endif
            runloop_frame_time = *info;
         }
         break;
      case RARCH_CTL_GET_WINDOWED_SCALE:
         {
            unsigned **scale = (unsigned**)data;
            if (!scale)
               return false;
            *scale       = (unsigned*)&runloop_pending_windowed_scale;
         }
         break;
      case RARCH_CTL_SET_WINDOWED_SCALE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            runloop_pending_windowed_scale = *idx;
         }
         break;
      case RARCH_CTL_SET_LIBRETRO_PATH:
         return path_set(RARCH_PATH_CORE, (const char*)data);
      case RARCH_CTL_FRAME_TIME_FREE:
         memset(&runloop_frame_time, 0,
               sizeof(struct retro_frame_time_callback));
         runloop_frame_time_last           = 0;
         runloop_max_frames                = 0;
         break;
      case RARCH_CTL_STATE_FREE:
         runloop_perfcnt_enable            = false;
         runloop_idle                      = false;
         runloop_paused                    = false;
         runloop_slowmotion                = false;
         runloop_overrides_active          = false;
         runloop_autosave                  = false;
         rarch_ctl(RARCH_CTL_FRAME_TIME_FREE, NULL);
         break;
      case RARCH_CTL_IS_IDLE:
         return runloop_idle;
      case RARCH_CTL_SET_IDLE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_idle = *ptr;
         }
         break;
      case RARCH_CTL_SET_PAUSED:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_paused = *ptr;
         }
         break;
      case RARCH_CTL_IS_PAUSED:
         return runloop_paused;
      case RARCH_CTL_TASK_INIT:
         {
#ifdef HAVE_THREADS
            settings_t *settings = config_get_ptr();
            bool threaded_enable = settings->bools.threaded_data_runloop_enable;
#else
            bool threaded_enable = false;
#endif
            task_queue_deinit();
            task_queue_init(threaded_enable, runloop_msg_queue_push);
         }
         break;
      case RARCH_CTL_SET_CORE_SHUTDOWN:
         runloop_core_shutdown_initiated = true;
         break;
      case RARCH_CTL_SET_SHUTDOWN:
         runloop_shutdown_initiated = true;
         break;
      case RARCH_CTL_IS_SHUTDOWN:
         return runloop_shutdown_initiated;
      case RARCH_CTL_DATA_DEINIT:
         task_queue_deinit();
         break;
      case RARCH_CTL_IS_CORE_OPTION_UPDATED:
         if (!runloop_core_options)
            return false;
         return  core_option_manager_updated(runloop_core_options);
      case RARCH_CTL_CORE_OPTION_PREV:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            core_option_manager_prev(runloop_core_options, *idx);
            if (ui_companion_is_on_foreground())
               ui_companion_driver_notify_refresh();
         }
         break;
      case RARCH_CTL_CORE_OPTION_NEXT:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            core_option_manager_next(runloop_core_options, *idx);
            if (ui_companion_is_on_foreground())
               ui_companion_driver_notify_refresh();
         }
         break;
      case RARCH_CTL_CORE_OPTIONS_GET:
         {
            struct retro_variable *var = (struct retro_variable*)data;

            if (!runloop_core_options || !var)
               return false;

            RARCH_LOG("Environ GET_VARIABLE %s:\n", var->key);
            core_option_manager_get(runloop_core_options, var);
            RARCH_LOG("\t%s\n", var->value ? var->value :
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
         }
         break;
      case RARCH_CTL_CORE_OPTIONS_INIT:
         {
            settings_t *settings              = config_get_ptr();
            char *game_options_path           = NULL;
            bool ret                          = false;
            const struct retro_variable *vars =
               (const struct retro_variable*)data;

            if (settings && settings->bools.game_specific_options)
               ret = rarch_game_specific_options(&game_options_path);

            if(ret)
            {
               runloop_game_options_active = true;
               runloop_core_options        =
                  core_option_manager_new(game_options_path, vars);
               free(game_options_path);
            }
            else
            {
               char buf[PATH_MAX_LENGTH];
               const char *options_path          = NULL;

               buf[0] = '\0';

               if (settings)
                  options_path = settings->paths.path_core_options;

               if (string_is_empty(options_path) && !path_is_empty(RARCH_PATH_CONFIG))
               {
                  fill_pathname_resolve_relative(buf, path_get(RARCH_PATH_CONFIG),
                        file_path_str(FILE_PATH_CORE_OPTIONS_CONFIG), sizeof(buf));
                  options_path = buf;
               }

               runloop_game_options_active = false;

               if (!string_is_empty(options_path))
                  runloop_core_options =
                     core_option_manager_new(options_path, vars);
            }

         }
         break;
      case RARCH_CTL_CORE_OPTIONS_DEINIT:
         {
            if (!runloop_core_options)
               return false;

            /* check if game options file was just created and flush
               to that file instead */
            if(!path_is_empty(RARCH_PATH_CORE_OPTIONS))
            {
               core_option_manager_flush_game_specific(runloop_core_options,
                     path_get(RARCH_PATH_CORE_OPTIONS));
               path_clear(RARCH_PATH_CORE_OPTIONS);
            }
            else
               core_option_manager_flush(runloop_core_options);

            if (runloop_game_options_active)
               runloop_game_options_active = false;

            if (runloop_core_options)
               core_option_manager_free(runloop_core_options);
            runloop_core_options          = NULL;
         }
         break;
      case RARCH_CTL_KEY_EVENT_GET:
         {
            retro_keyboard_event_t **key_event =
               (retro_keyboard_event_t**)data;
            if (!key_event)
               return false;
            *key_event = &runloop_key_event;
         }
         break;
      case RARCH_CTL_FRONTEND_KEY_EVENT_GET:
         {
            retro_keyboard_event_t **key_event =
               (retro_keyboard_event_t**)data;
            if (!key_event)
               return false;
            *key_event = &runloop_frontend_key_event;
         }
         break;
      case RARCH_CTL_HTTPSERVER_INIT:
#if defined(HAVE_HTTPSERVER) && defined(HAVE_ZLIB)
         httpserver_init(8888);
#endif
         break;
      case RARCH_CTL_HTTPSERVER_DESTROY:
#if defined(HAVE_HTTPSERVER) && defined(HAVE_ZLIB)
         httpserver_destroy();
#endif
         break;
      case RARCH_CTL_NONE:
      default:
         return false;
   }

   return true;
}

bool retroarch_is_forced_fullscreen(void)
{
   return rarch_force_fullscreen;
}

void retroarch_unset_forced_fullscreen(void)
{
   rarch_force_fullscreen = false;
}

/* set a runtime shader preset without overwriting the settings value */
void retroarch_set_shader_preset(const char* preset)
{
   if (!string_is_empty(preset))
      strlcpy(runtime_shader_preset, preset, sizeof(runtime_shader_preset));
   else
      runtime_shader_preset[0] = '\0';
}

/* unset a runtime shader preset */
void retroarch_unset_shader_preset(void)
{
   runtime_shader_preset[0] = '\0';
}

/* get the name of the current shader preset */
char* retroarch_get_shader_preset(void)
{
   settings_t *settings = config_get_ptr();
   if (!settings->bools.video_shader_enable)
      return NULL;

   if (!string_is_empty(runtime_shader_preset))
      return runtime_shader_preset;
   else if (!string_is_empty(settings->paths.path_shader))
      return settings->paths.path_shader;
   else
      return NULL;
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
               return BIT256_GET(has_set_libretro_device, bit);
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
               BIT256_SET(has_set_libretro_device, bit);
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
               BIT256_CLEAR(has_set_libretro_device, bit);
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
   retro_assert(rarch_error_on_init);

   strlcpy(error_string, error, sizeof(error_string));
   longjmp(error_sjlj_context, error_code);
}

bool retroarch_main_quit(void)
{
   if (!runloop_shutdown_initiated)
   {
      command_event(CMD_EVENT_AUTOSAVE_STATE, NULL);
      command_event(CMD_EVENT_DISABLE_OVERRIDES, NULL);
      command_event(CMD_EVENT_RESTORE_DEFAULT_SHADER_PRESET, NULL);
      command_event(CMD_EVENT_RESTORE_REMAPS, NULL);
   }

   runloop_shutdown_initiated = true;
   rarch_menu_running_finished();

   return true;
}

global_t *global_get_ptr(void)
{
   static struct global g_extern;
   return &g_extern;
}


void runloop_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush)
{
   runloop_ctx_msg_info_t msg_info;

#ifdef HAVE_THREADS
   runloop_msg_queue_lock();
#endif

   if (flush)
      msg_queue_clear(runloop_msg_queue);

   msg_info.msg      = msg;
   msg_info.prio     = prio;
   msg_info.duration = duration;
   msg_info.flush    = flush;

   if (runloop_msg_queue)
   {
      msg_queue_push(runloop_msg_queue, msg_info.msg,
            msg_info.prio, msg_info.duration);

      if (ui_companion_is_on_foreground())
      {
         const ui_companion_driver_t *ui = ui_companion_get_ptr();
         if (ui->msg_queue_push)
            ui->msg_queue_push(msg_info.msg,
                  msg_info.prio, msg_info.duration, msg_info.flush);
      }
   }

#ifdef HAVE_THREADS
   runloop_msg_queue_unlock();
#endif
}


void runloop_get_status(bool *is_paused, bool *is_idle,
      bool *is_slowmotion, bool *is_perfcnt_enable)
{
   *is_paused         = runloop_paused;
   *is_idle           = runloop_idle;
   *is_slowmotion     = runloop_slowmotion;
   *is_perfcnt_enable = runloop_perfcnt_enable;
}

bool runloop_msg_queue_pull(const char **ret)
{
#ifdef HAVE_THREADS
   runloop_msg_queue_lock();
#endif
   if (!ret)
      return false;
   *ret = msg_queue_pull(runloop_msg_queue);
#ifdef HAVE_THREADS
   runloop_msg_queue_unlock();
#endif
   return true;
}

/* Time to exit out of the main loop?
 * Reasons for exiting:
 * a) Shutdown environment callback was invoked.
 * b) Quit key was pressed.
 * c) Frame count exceeds or equals maximum amount of frames to run.
 * d) Video driver no longer alive.
 * e) End of BSV movie and BSV EOF exit is true. (TODO/FIXME - explain better)
 */
#define time_to_exit(quit_key_pressed) (runloop_shutdown_initiated || quit_key_pressed || !is_alive || bsv_movie_is_end_of_file() || ((runloop_max_frames != 0) && (frame_count >= runloop_max_frames)) || runloop_exec)

#define runloop_check_cheevos() (settings->bools.cheevos_enable && cheevos_loaded && (!cheats_are_enabled && !cheats_were_enabled))

#ifdef HAVE_NETWORKING
/* FIXME: This is an ugly way to tell Netplay this... */
#define runloop_netplay_pause() netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL)
#else
#define runloop_netplay_pause() ((void)0)
#endif

#ifdef HAVE_MENU
static bool input_driver_toggle_button_combo(
      unsigned mode, retro_bits_t* p_input)
{
   switch (mode)
   {
      case INPUT_TOGGLE_DOWN_Y_L_R:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_Y))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_L3_R3:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R3))
            return false;
         break;
      case INPUT_TOGGLE_L1_R1_START_SELECT:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_START_SELECT:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         break;
      default:
      case INPUT_TOGGLE_NONE:
         return false;
   }

   return true;
}
#endif

static enum runloop_state runloop_check_state(
      settings_t *settings,
      bool input_nonblock_state,
      unsigned *sleep_ms)
{
   retro_bits_t current_input;
#ifdef HAVE_MENU
   static retro_bits_t last_input   = {{0}};
#endif
   static bool old_fs_toggle_pressed= false;
   static bool old_focus            = true;
   bool is_focused                  = false;
   bool is_alive                    = false;
   uint64_t frame_count             = 0;
   bool focused                     = true;
   bool pause_nonactive             = settings->bools.pause_nonactive;
   bool fs_toggle_triggered         = false;
#ifdef HAVE_MENU
   bool menu_driver_binding_state   = menu_driver_is_binding_state();
   bool menu_is_alive               = menu_driver_is_alive();

   if (menu_is_alive && !(settings->bools.menu_unified_controls && !menu_input_dialog_get_display_kb()))
	   input_menu_keys_pressed(settings, &current_input);
   else
#endif
	   input_keys_pressed(settings, &current_input);

#ifdef HAVE_MENU
   last_input                       = current_input;
   if (
         ((settings->uints.input_menu_toggle_gamepad_combo != INPUT_TOGGLE_NONE) &&
          input_driver_toggle_button_combo(
             settings->uints.input_menu_toggle_gamepad_combo, &last_input)))
      BIT256_SET(current_input, RARCH_MENU_TOGGLE);
#endif

   if (input_driver_flushing_input)
   {
      input_driver_flushing_input = false;
      if (bits_any_set(current_input.data, ARRAY_SIZE(current_input.data)))
      {
         BIT256_CLEAR_ALL(current_input);
         if (runloop_paused)
            BIT256_SET(current_input, RARCH_PAUSE_TOGGLE);
         input_driver_flushing_input = true;
      }
   }

   video_driver_get_status(&frame_count, &is_alive, &is_focused);

#ifdef HAVE_MENU
   if (menu_driver_binding_state)
      BIT256_CLEAR_ALL(current_input);
#endif

#ifdef HAVE_OVERLAY
   /* Check next overlay */
   {
      static bool old_should_check_next_overlay = false;
      bool should_check_next_overlay            = BIT256_GET(
            current_input, RARCH_OVERLAY_NEXT);

      if (should_check_next_overlay && !old_should_check_next_overlay)
         command_event(CMD_EVENT_OVERLAY_NEXT, NULL);

      old_should_check_next_overlay             = should_check_next_overlay;
   }
#endif

   /* Check fullscreen toggle */
   {
      bool fs_toggle_pressed = BIT256_GET(
            current_input, RARCH_FULLSCREEN_TOGGLE_KEY);
      fs_toggle_triggered    = fs_toggle_pressed && !old_fs_toggle_pressed;

      if (fs_toggle_triggered)
      {
         bool fullscreen_toggled = !runloop_paused
#ifdef HAVE_MENU
            || menu_is_alive;
#else
;
#endif

         if (fullscreen_toggled)
            command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);
      }

      old_fs_toggle_pressed = fs_toggle_pressed;
   }

   /* Check mouse grab toggle */
   {
      static bool old_pressed = false;
      bool pressed            = BIT256_GET(
            current_input, RARCH_GRAB_MOUSE_TOGGLE);

      if (pressed && !old_pressed)
#if HAVE_LIBUI
         command_event(CMD_EVENT_LIBUI_TEST, NULL);
#else
         command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
#endif

      old_pressed             = pressed;
   }


#ifdef HAVE_OVERLAY
   {
      static char prev_overlay_restore = false;
      if (input_keyboard_ctl(
               RARCH_INPUT_KEYBOARD_CTL_IS_LINEFEED_ENABLED, NULL))
      {
         prev_overlay_restore  = false;
         command_event(CMD_EVENT_OVERLAY_INIT, NULL);
      }
      else if (prev_overlay_restore)
      {
         if (!settings->bools.input_overlay_hide_in_menu)
            command_event(CMD_EVENT_OVERLAY_INIT, NULL);
         prev_overlay_restore = false;
      }
   }
#endif

   /* Check quit key */
   {
      static bool old_quit_key = false;
      bool quit_key            = BIT256_GET(
            current_input, RARCH_QUIT_KEY);
      bool trig_quit_key       = quit_key && !old_quit_key;

      old_quit_key             = quit_key;

      if (time_to_exit(trig_quit_key))
      {
         if (runloop_exec)
            runloop_exec = false;

         if (runloop_core_shutdown_initiated && settings->bools.load_dummy_on_core_shutdown)
         {
            content_ctx_info_t content_info;

            content_info.argc               = 0;
            content_info.argv               = NULL;
            content_info.args               = NULL;
            content_info.environ_get        = NULL;

            if (!task_push_start_dummy_core(&content_info))
            {
               old_quit_key                 = quit_key;
               retroarch_main_quit();
               return RUNLOOP_STATE_QUIT;
            }

            /* Loads dummy core instead of exiting RetroArch completely.
             * Aborts core shutdown if invoked. */
            runloop_shutdown_initiated      = false;
            runloop_core_shutdown_initiated = false;
         }
         else
         {
            old_quit_key                 = quit_key;
            retroarch_main_quit();
            return RUNLOOP_STATE_QUIT;
         }
      }
   }

#ifdef HAVE_MENU
   if (menu_is_alive)
   {
      static retro_bits_t old_input = {{0}};
      menu_ctx_iterate_t iter;

      retro_ctx.poll_cb();

      {
         enum menu_action action;
         bool focused               = false;
         retro_bits_t trigger_input = current_input;

         bits_clear_bits(trigger_input.data, old_input.data,
               ARRAY_SIZE(trigger_input.data));

         action   = (enum menu_action)menu_event(&current_input, &trigger_input);
         focused              = pause_nonactive ? is_focused : true;

         focused                   = focused && !ui_companion_is_on_foreground();

         iter.action               = action;

         if (!menu_driver_iterate(&iter))
            rarch_menu_running_finished();

         if (focused || !runloop_idle)
            menu_driver_render(runloop_idle, rarch_is_inited,
                  (current_core_type == CORE_TYPE_DUMMY)
                  )
               ;

         old_input                 = current_input;

         if (!focused)
            return RUNLOOP_STATE_POLLED_AND_SLEEP;

         if (action == MENU_ACTION_QUIT && !menu_driver_binding_state)
            return RUNLOOP_STATE_QUIT;
      }

      if (runloop_idle)
         return RUNLOOP_STATE_POLLED_AND_SLEEP;
   }
   else
#endif
      if (runloop_idle)
         return RUNLOOP_STATE_SLEEP;


   /* Check game focus toggle */
   {
      static bool old_pressed = false;
      bool pressed            = BIT256_GET(
            current_input, RARCH_GAME_FOCUS_TOGGLE);

      if (pressed && !old_pressed)
         command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, (void*)(intptr_t)0);

      old_pressed             = pressed;
   }

#ifdef HAVE_MENU
   /* Check menu toggle */
   {
      static bool old_pressed = false;
      bool pressed            = BIT256_GET(
            current_input, RARCH_MENU_TOGGLE);

      if (menu_event_kb_is_set(RETROK_F1) == 1)
      {
         if (menu_driver_is_alive())
         {
            if (rarch_is_inited && (current_core_type != CORE_TYPE_DUMMY))
            {
               rarch_menu_running_finished();
               menu_event_kb_set(false, RETROK_F1);
            }
         }
      }
      else if ((!menu_event_kb_is_set(RETROK_F1) &&
               (pressed && !old_pressed)) ||
            (current_core_type == CORE_TYPE_DUMMY))
      {
         if (menu_driver_is_alive())
         {
            if (rarch_is_inited && (current_core_type != CORE_TYPE_DUMMY))
               rarch_menu_running_finished();
         }
         else
         {
            menu_display_toggle_set_reason(MENU_TOGGLE_REASON_USER);
            rarch_menu_running();
         }
      }
      else
         menu_event_kb_set(false, RETROK_F1);

      old_pressed             = pressed;
   }

   if (menu_driver_is_alive())
   {
      if (!settings->bools.menu_throttle_framerate && !settings->floats.fastforward_ratio)
         return RUNLOOP_STATE_MENU_ITERATE;

      return RUNLOOP_STATE_END;
   }
#endif

   if (pause_nonactive)
      focused                = is_focused;

   /* Check screenshot toggle */
   {
      static bool old_pressed = false;
      bool pressed            = BIT256_GET(
            current_input, RARCH_SCREENSHOT);

      if (pressed && old_pressed)
         command_event(CMD_EVENT_TAKE_SCREENSHOT, NULL);

      old_pressed             = pressed;
   }

   /* Check audio mute toggle */
   {
      static bool old_pressed = false;
      bool pressed            = BIT256_GET(
            current_input, RARCH_MUTE);

      if (pressed && !old_pressed)
         command_event(CMD_EVENT_AUDIO_MUTE_TOGGLE, NULL);

      old_pressed             = pressed;
   }

   /* Check OSK toggle */
   {
      static bool old_pressed = false;
      bool pressed            = BIT256_GET(current_input, RARCH_OSK);

      if (pressed && !old_pressed)
      {
         if (input_keyboard_ctl(
                  RARCH_INPUT_KEYBOARD_CTL_IS_LINEFEED_ENABLED, NULL))
            input_keyboard_ctl(
                  RARCH_INPUT_KEYBOARD_CTL_UNSET_LINEFEED_ENABLED, NULL);
         else
            input_keyboard_ctl(
                  RARCH_INPUT_KEYBOARD_CTL_SET_LINEFEED_ENABLED, NULL);
      }

      old_pressed             = pressed;
   }

   if (BIT256_GET(current_input, RARCH_VOLUME_UP))
      command_event(CMD_EVENT_VOLUME_UP, NULL);
   else if (BIT256_GET(current_input, RARCH_VOLUME_DOWN))
      command_event(CMD_EVENT_VOLUME_DOWN, NULL);

#ifdef HAVE_NETWORKING
   /* Check Netplay */
   {
      static bool old_netplay_watch = false;
      bool netplay_watch            = BIT256_GET(
            current_input, RARCH_NETPLAY_GAME_WATCH);

      if (netplay_watch && !old_netplay_watch)
         netplay_driver_ctl(RARCH_NETPLAY_CTL_GAME_WATCH, NULL);

      old_netplay_watch             = netplay_watch;
   }
#endif

   /* Check pause */
   {
      static bool old_frameadvance  = false;
      static bool old_pause_pressed = false;
      bool check_is_oneshot         = true;
      bool frameadvance_pressed     = BIT256_GET(
            current_input, RARCH_FRAMEADVANCE);
      bool pause_pressed            = BIT256_GET(
            current_input, RARCH_PAUSE_TOGGLE);
      bool trig_frameadvance        = frameadvance_pressed && !old_frameadvance;

      /* Check if libretro pause key was pressed. If so, pause or
       * unpause the libretro core. */

      /* FRAMEADVANCE will set us into pause mode. */
      pause_pressed                |= !runloop_paused && trig_frameadvance;

      if (focused && pause_pressed && !old_pause_pressed)
         command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
      else if (focused && !old_focus)
         command_event(CMD_EVENT_UNPAUSE, NULL);
      else if (!focused && old_focus)
         command_event(CMD_EVENT_PAUSE, NULL);

      old_focus           = focused;
      old_pause_pressed   = pause_pressed;
      old_frameadvance    = frameadvance_pressed;

      if (runloop_paused)
      {
         check_is_oneshot = trig_frameadvance ||
            BIT256_GET(current_input, RARCH_REWIND);

         if (fs_toggle_triggered)
         {
            command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);
            if (!runloop_idle)
               video_driver_cached_frame();
         }
      }

      if (!check_is_oneshot)
         return RUNLOOP_STATE_SLEEP;
   }

   if (!focused)
      return RUNLOOP_STATE_SLEEP;

   /* Check fast forward button */
   /* To avoid continous switching if we hold the button down, we require
    * that the button must go from pressed to unpressed back to pressed
    * to be able to toggle between then.
    */
   {
      static bool old_button_state      = false;
      static bool old_hold_button_state = false;
      bool new_button_state             = BIT256_GET(
            current_input, RARCH_FAST_FORWARD_KEY);
      bool new_hold_button_state        = BIT256_GET(
            current_input, RARCH_FAST_FORWARD_HOLD_KEY);

      if (new_button_state && !old_button_state)
      {
         if (input_nonblock_state) {
            input_driver_unset_nonblock_state();
            runloop_fastmotion = false;
         }
         else {
            input_driver_set_nonblock_state();
            runloop_fastmotion = true;
         }
         driver_set_nonblock_state();
      }
      else if (old_hold_button_state != new_hold_button_state)
      {
         if (new_hold_button_state) {
            input_driver_set_nonblock_state();
            runloop_fastmotion = true;
         }
         else {
            input_driver_unset_nonblock_state();
            runloop_fastmotion = false;
         }
         driver_set_nonblock_state();
      }

      /* Display the fast forward state to the user, if needed. */
      if (runloop_fastmotion)
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAST_FORWARD), 1, 1, false);

      old_button_state                  = new_button_state;
      old_hold_button_state             = new_hold_button_state;
   }

   /* Check state slots */
   {
      static bool old_should_slot_increase = false;
      static bool old_should_slot_decrease = false;
      bool should_slot_increase            = BIT256_GET(
            current_input, RARCH_STATE_SLOT_PLUS);
      bool should_slot_decrease            = BIT256_GET(
            current_input, RARCH_STATE_SLOT_MINUS);

      /* Checks if the state increase/decrease keys have been pressed
       * for this frame. */
      if (should_slot_increase && !old_should_slot_increase)
      {
         char msg[128];
         int new_state_slot = settings->ints.state_slot + 1;

         msg[0] = '\0';

         configuration_set_int(settings, settings->ints.state_slot, new_state_slot);

         snprintf(msg, sizeof(msg), "%s: %d",
               msg_hash_to_str(MSG_STATE_SLOT),
               settings->ints.state_slot);

         runloop_msg_queue_push(msg, 2, 180, true);

         RARCH_LOG("%s\n", msg);
      }
      else if (should_slot_decrease && !old_should_slot_decrease)
      {
         char msg[128];
         int new_state_slot = settings->ints.state_slot - 1;

         msg[0] = '\0';

         if (settings->ints.state_slot > 0)
         {
            configuration_set_int(settings, settings->ints.state_slot, new_state_slot);
         }

         snprintf(msg, sizeof(msg), "%s: %d",
               msg_hash_to_str(MSG_STATE_SLOT),
               settings->ints.state_slot);

         runloop_msg_queue_push(msg, 2, 180, true);

         RARCH_LOG("%s\n", msg);
      }

      old_should_slot_increase = should_slot_increase;
      old_should_slot_decrease = should_slot_decrease;
   }

   /* Check savestates */
   {
      static bool old_should_savestate = false;
      static bool old_should_loadstate = false;
      bool should_savestate            = BIT256_GET(
            current_input, RARCH_SAVE_STATE_KEY);
      bool should_loadstate            = BIT256_GET(
            current_input, RARCH_LOAD_STATE_KEY);

      if (should_savestate && !old_should_savestate)
         command_event(CMD_EVENT_SAVE_STATE, NULL);
      if (should_loadstate && !old_should_loadstate)
         command_event(CMD_EVENT_LOAD_STATE, NULL);

      old_should_savestate             = should_savestate;
      old_should_loadstate             = should_loadstate;
   }

#ifdef HAVE_CHEEVOS
   if (!settings->bools.cheevos_hardcore_mode_enable)
#endif
   {
      char s[128];
      unsigned t = 0;

      s[0] = '\0';

      if (state_manager_check_rewind(BIT256_GET(current_input, RARCH_REWIND),
            settings->uints.rewind_granularity, runloop_paused, s, sizeof(s), &t))
         runloop_msg_queue_push(s, 0, t, true);
   }

   /* Checks if slowmotion toggle/hold was being pressed and/or held. */
   {
      runloop_slowmotion = BIT256_GET(current_input, RARCH_SLOWMOTION);

      if (runloop_slowmotion)
      {
         if (settings->bools.video_black_frame_insertion)
         {
            if (!runloop_idle)
               video_driver_cached_frame();
         }

         if (state_manager_frame_is_reversed())
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_SLOW_MOTION_REWIND), 1, 1, false);
         else
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_SLOW_MOTION), 1, 1, false);
      }
   }

   /* Check movie record toggle */
   {
      static bool old_pressed = false;
      bool pressed            = BIT256_GET(
            current_input, RARCH_MOVIE_RECORD_TOGGLE);

      if (pressed && !old_pressed)
         bsv_movie_check();

      old_pressed             = pressed;
   }

   /* Check shader prev/next */
   {
      static bool old_shader_next = false;
      static bool old_shader_prev = false;
      bool shader_next            = BIT256_GET(
            current_input, RARCH_SHADER_NEXT);
      bool shader_prev            = BIT256_GET(
            current_input, RARCH_SHADER_PREV);
      bool trig_shader_next       = shader_next && !old_shader_next;
      bool trig_shader_prev       = shader_prev && !old_shader_prev;

      if (trig_shader_next || trig_shader_prev)
         dir_check_shader(trig_shader_next, trig_shader_prev);

      old_shader_next             = shader_next;
      old_shader_prev             = shader_prev;
   }

   /* Check disk */
   {
      static bool old_disk_eject  = false;
      static bool old_disk_next   = false;
      static bool old_disk_prev   = false;
      bool disk_eject             = BIT256_GET(
            current_input, RARCH_DISK_EJECT_TOGGLE);
      bool disk_next              = BIT256_GET(
            current_input, RARCH_DISK_NEXT);
      bool disk_prev              = BIT256_GET(
            current_input, RARCH_DISK_PREV);

      if (disk_eject && !old_disk_eject)
         command_event(CMD_EVENT_DISK_EJECT_TOGGLE, NULL);
      else if (disk_next && !old_disk_next)
         command_event(CMD_EVENT_DISK_NEXT, NULL);
      else if (disk_prev && !old_disk_prev)
         command_event(CMD_EVENT_DISK_PREV, NULL);

      old_disk_eject              = disk_eject;
      old_disk_prev               = disk_prev;
      old_disk_next               = disk_next;
   }

   /* Check reset */
   {
      static bool old_state = false;
      bool new_state        = BIT256_GET(
            current_input, RARCH_RESET);

      if (new_state && !old_state)
         command_event(CMD_EVENT_RESET, NULL);

      old_state = new_state;
   }

   /* Check cheats */
   {
      static bool old_cheat_index_plus   = false;
      static bool old_cheat_index_minus  = false;
      static bool old_cheat_index_toggle = false;
      bool cheat_index_plus              = BIT256_GET(
            current_input, RARCH_CHEAT_INDEX_PLUS);
      bool cheat_index_minus             = BIT256_GET(
            current_input, RARCH_CHEAT_INDEX_MINUS);
      bool cheat_index_toggle            = BIT256_GET(
            current_input, RARCH_CHEAT_TOGGLE);

      if (cheat_index_plus && !old_cheat_index_plus)
         cheat_manager_index_next();
      else if (cheat_index_minus && !old_cheat_index_minus)
         cheat_manager_index_prev();
      else if (cheat_index_toggle && !old_cheat_index_toggle)
         cheat_manager_toggle();

      old_cheat_index_plus               = cheat_index_plus;
      old_cheat_index_minus              = cheat_index_minus;
      old_cheat_index_toggle             = cheat_index_toggle;
   }

   if (settings->bools.video_shader_watch_files)
   {
      static rarch_timer_t timer = {0};
      static bool need_to_apply = false;

      if (video_shader_check_for_changes())
      {
         need_to_apply = true;

         if (!rarch_timer_is_running(&timer))
         {
            /* rarch_timer_t convenience functions only support whole seconds. */

            /* rarch_timer_begin */
            timer.timeout_end = cpu_features_get_time_usec() + SHADER_FILE_WATCH_DELAY_MSEC * 1000;
            timer.timer_begin = true;
            timer.timer_end = false;
         }
      }

      /* If a file is modified atomically (moved/renamed from a different file), we have no idea how long that might take.
       * If we're trying to re-apply shaders immediately after changes are made to the original file(s), the filesystem might be in an in-between
       * state where the new file hasn't been moved over yet and the original file was already deleted. This leaves us no choice
       * but to wait an arbitrary amount of time and hope for the best.
       */
      if (need_to_apply)
      {
         /* rarch_timer_tick */
         timer.current = cpu_features_get_time_usec();
         timer.timeout = (timer.timeout_end - timer.current) / 1000;

         if (!timer.timer_end && rarch_timer_has_expired(&timer))
         {
            rarch_timer_end(&timer);
            need_to_apply = false;
            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }

   return RUNLOOP_STATE_ITERATE;
}

void runloop_set(enum runloop_action action)
{
   switch (action)
   {
      case RUNLOOP_ACTION_AUTOSAVE:
         runloop_autosave = true;
         break;
      case RUNLOOP_ACTION_NONE:
         break;
   }
}

void runloop_unset(enum runloop_action action)
{
   switch (action)
   {
      case RUNLOOP_ACTION_AUTOSAVE:
         runloop_autosave = false;
         break;
      case RUNLOOP_ACTION_NONE:
         break;
   }
}

/**
 * runloop_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on success, 1 if we have to wait until
 * button input in order to wake up the loop,
 * -1 if we forcibly quit out of the RetroArch iteration loop.
 **/
int runloop_iterate(unsigned *sleep_ms)
{
   unsigned i;
   bool input_nonblock_state                    = input_driver_is_nonblock_state();
   settings_t *settings                         = config_get_ptr();
   unsigned max_users                           = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));

   if (runloop_frame_time.callback)
   {
      /* Updates frame timing if frame timing callback is in use by the core.
       * Limits frame time if fast forward ratio throttle is enabled. */

      retro_time_t current     = cpu_features_get_time_usec();
      retro_time_t delta       = current - runloop_frame_time_last;
      bool is_locked_fps       = (runloop_paused ||
                                  input_nonblock_state) |
                                  !!recording_data;


      if (!runloop_frame_time_last || is_locked_fps)
         delta = runloop_frame_time.reference;

      if (!is_locked_fps && runloop_slowmotion)
         delta /= settings->floats.slowmotion_ratio;

      runloop_frame_time_last = current;

      if (is_locked_fps)
         runloop_frame_time_last = 0;

      runloop_frame_time.callback(delta);
   }

   switch ((enum runloop_state)
         runloop_check_state(
            settings,
            input_nonblock_state,
            sleep_ms))
   {
      case RUNLOOP_STATE_QUIT:
         frame_limit_last_time = 0.0;
         command_event(CMD_EVENT_QUIT, NULL);
         return -1;
      case RUNLOOP_STATE_POLLED_AND_SLEEP:
         runloop_netplay_pause();
         *sleep_ms = 10;
         return 1;
      case RUNLOOP_STATE_SLEEP:
         retro_ctx.poll_cb();
         runloop_netplay_pause();
         *sleep_ms = 10;
         return 1;
      case RUNLOOP_STATE_END:
         runloop_netplay_pause();
         goto end;
      case RUNLOOP_STATE_MENU_ITERATE:
         runloop_netplay_pause();
         return 0;
      case RUNLOOP_STATE_ITERATE:
         break;
   }

   if (runloop_autosave)
      autosave_lock();

   bsv_movie_set_frame_start();

   camera_driver_poll();

   /* Update binds for analog dpad modes. */
   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *general_binds = input_config_binds[i];
      struct retro_keybind *auto_binds    = input_autoconf_binds[i];
      enum analog_dpad_mode dpad_mode     = (enum analog_dpad_mode)settings->uints.input_analog_dpad_mode[i];

      if (dpad_mode == ANALOG_DPAD_NONE)
         continue;

      input_push_analog_dpad(general_binds, dpad_mode);
      input_push_analog_dpad(auto_binds,    dpad_mode);
   }

   if ((settings->uints.video_frame_delay > 0) && !input_nonblock_state)
      retro_sleep(settings->uints.video_frame_delay);

   core_run();

#ifdef HAVE_CHEEVOS
   if (runloop_check_cheevos())
      cheevos_test();
#endif

   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *general_binds = input_config_binds[i];
      struct retro_keybind *auto_binds    = input_autoconf_binds[i];
      enum analog_dpad_mode dpad_mode     = (enum analog_dpad_mode)settings->uints.input_analog_dpad_mode[i];

      if (dpad_mode == ANALOG_DPAD_NONE)
         continue;

      input_pop_analog_dpad(general_binds);
      input_pop_analog_dpad(auto_binds);
   }

   bsv_movie_set_frame_end();

   if (runloop_autosave)
      autosave_unlock();

   if (settings->floats.fastforward_ratio)
      end:
   {
      retro_time_t to_sleep_ms  = (
            (frame_limit_last_time + frame_limit_minimum_time)
            - cpu_features_get_time_usec()) / 1000;

      if (to_sleep_ms > 0)
      {
         *sleep_ms = (unsigned)to_sleep_ms;
         /* Combat jitter a bit. */
         frame_limit_last_time += frame_limit_minimum_time;
         return 1;
      }

      frame_limit_last_time  = cpu_features_get_time_usec();
   }

   return 0;
}

rarch_system_info_t *runloop_get_system_info(void)
{
   return &runloop_system;
}

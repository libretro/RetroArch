/*  RetroArch - A frontend for libretro.
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

/* The GameCube and Wii on os/gekko: storage mounted as C library
 * devices, the directory layout under the one RetroArch started from,
 * and leaving through the loader.
 *
 * Wii: the front SD slot is "sd:", the first USB drive "usb:"; both
 * can be taken out and put back while RetroArch runs.
 * GameCube: an SD adapter in serial port 2 is "sd:", in the memory
 * card slots "carda:" and "cardb:".
 *
 * Switching cores runs the other core's program, with the content as
 * the directory and file name loaders pass. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gekko/disk.h>
#include <gekko/exec.h>
#ifdef HW_RVL
#include <gekko/thread.h>
#include <gekko/usb.h>
#endif

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../frontend_driver.h"

#include "../../defaults.h"
#include "../../msg_hash.h"
#include "../../retroarch_types.h"
#include "../../verbosity.h"

#if !defined(IS_SALAMANDER)
#include "../../paths.h"
#include "../../menu/menu_entries.h"
#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#endif

/* The core, and up to what netplay passes. */
#define EXEC_MAX_ARGS 32
#endif

struct volume
{
   const char *name;
   uint8_t     open;      /* the device */
   uint8_t     mounted;
};

#ifdef HW_RVL
enum { VOL_SD = 0, VOL_USB, VOL_COUNT };
static struct volume volumes[VOL_COUNT] = { { "sd", 0, 0 }, { "usb", 0, 0 } };

static gk_blockdev_t *vol_open(unsigned i)
{
   return i == VOL_SD ? gk_sd_open() : gk_usbstorage_open();
}
#else
enum { VOL_SD = 0, VOL_CARDA, VOL_CARDB, VOL_COUNT };
static struct volume volumes[VOL_COUNT] = {
   { "sd", 0, 0 }, { "carda", 0, 0 }, { "cardb", 0, 0 } };
/* The EXI channel each GameCube volume sits on. */
static const unsigned volume_channel[VOL_COUNT] = { 2, 0, 1 };

static gk_blockdev_t *vol_open(unsigned i)
{
   return gk_sdgecko_open(volume_channel[i]);
}
#endif

static void attach(unsigned i)
{
   gk_blockdev_t *dev = vol_open(i);
   int ret;
   if (!dev)
      return;
   volumes[i].open = 1;
   if ((ret = gk_fat_mount(volumes[i].name, dev)))
      RARCH_WARN("[Gekko] %s: no FAT volume (%d).\n", volumes[i].name, ret);
   else
      volumes[i].mounted = 1;
}

static void unmount_all(void)
{
   unsigned i;
   for (i = 0; i < VOL_COUNT; i++)
      if (volumes[i].mounted)
      {
         gk_fat_unmount(volumes[i].name);
         volumes[i].mounted = 0;
      }
}

#if defined(HW_RVL) && !defined(IS_SALAMANDER)
static gk_thread_t       *watcher;
static volatile uint32_t  watch_quit;

/* Once a second: a drive taken out is unmounted, files still open on
 * it fail; one put in is mounted. */
static void *watch(void *arg)
{
   (void)arg;
   while (!watch_quit)
   {
      unsigned i;
      for (i = 0; i < VOL_COUNT && !watch_quit; i++)
      {
         if (!volumes[i].open)
         {
            attach(i);
            if (volumes[i].mounted)
               RARCH_LOG("[Gekko] %s: mounted.\n", volumes[i].name);
         }
         else if (!(i == VOL_SD ? gk_sd_inserted() : gk_usbstorage_inserted()))
         {
            RARCH_LOG("[Gekko] %s: taken out.\n", volumes[i].name);
            if (volumes[i].mounted)
               gk_fat_unmount(volumes[i].name);
            volumes[i].mounted = 0;
            if (i == VOL_SD)
               gk_sd_close();
            else
               gk_usbstorage_close();
            volumes[i].open = 0;
         }
      }
      gk_futex_wait(&watch_quit, 0, GK_US_TO_TICKS(1000000));
   }
   return NULL;
}
#endif

static void frontend_gekko_init(void *data)
{
   unsigned i;
   (void)data;
   for (i = 0; i < VOL_COUNT; i++)
      attach(i);
#if defined(HW_RVL) && !defined(IS_SALAMANDER)
   watch_quit = 0;
   watcher    = gk_thread_create(watch, NULL, NULL, 16384, GK_PRIO_DEFAULT);
#endif
}

#ifdef IS_SALAMANDER
static char content_dir[PATH_MAX_LENGTH];
static char content_file[PATH_MAX_LENGTH];

/* The core the configuration names, with the loader's content. */
static void frontend_gekko_exitspawn(char *s, size_t len, char *args)
{
   char core[PATH_MAX_LENGTH];
   const char *argv[3];
   void *image  = NULL;
   int64_t size = 0;
   int argc     = 1;
   int ret;
   (void)len;
   (void)args;
   if (string_is_empty(s))
      return;
   if (strchr(s, ':'))
      strlcpy(core, s, sizeof(core));
   else
      fill_pathname_join(core, g_defaults.dirs[DEFAULT_DIR_CORE], s,
            sizeof(core));
   argv[0] = core;
   if (*content_file)
   {
      argv[1] = content_dir;
      argv[2] = content_file;
      argc    = 3;
   }
   if (!filestream_read_file(core, &image, &size))
   {
      RARCH_ERR("[Gekko] Could not read \"%s\".\n", core);
      return;
   }
   unmount_all();
   ret = gk_exec(image, (size_t)size, argc, argv);
   RARCH_ERR("[Gekko] Could not run \"%s\" (%d).\n", core, ret);
   free(image);
}
#else
static enum frontend_fork fork_mode = FRONTEND_FORK_NONE;
static void              *exec_image;
static int64_t            exec_len;
static int                exec_argc;
static const char        *exec_argv[EXEC_MAX_ARGS];
static char               exec_core[PATH_MAX_LENGTH];
/* The rest of the arguments, one after another. */
static char               exec_args[PATH_MAX_LENGTH * 2];

static void exec_arg(const char *arg, size_t len, size_t *used)
{
   if (     exec_argc >= EXEC_MAX_ARGS
         || *used + len + 1 > sizeof(exec_args))
      return;
   memcpy(exec_args + *used, arg, len);
   exec_args[*used + len] = '\0';
   exec_argv[exec_argc++] = exec_args + *used;
   *used                 += len + 1;
}

/* The next program, read while the volumes are still mounted. */
static void exec_prepare(void)
{
   const char *core = path_get(RARCH_PATH_CORE);
   size_t used      = 0;
   if (fork_mode == FRONTEND_FORK_NONE || string_is_empty(core))
      return;
   if (strchr(core, ':'))
      strlcpy(exec_core, core, sizeof(exec_core));
   else
      fill_pathname_join(exec_core, g_defaults.dirs[DEFAULT_DIR_CORE],
            core, sizeof(exec_core));
   exec_argv[0] = exec_core;
   exec_argc    = 1;
   if (fork_mode == FRONTEND_FORK_CORE_WITH_ARGS)
   {
#ifdef HAVE_NETWORKING
      /* Netplay passes RetroArch's own options. */
      char *fork_args[NETPLAY_FORK_MAX_ARGS];
      if (netplay_driver_ctl(RARCH_NETPLAY_CTL_GET_FORK_ARGS, fork_args))
      {
         unsigned i;
         for (i = 0; fork_args[i]; i++)
            exec_arg(fork_args[i], strlen(fork_args[i]), &used);
      }
      else
#endif
      if (!path_is_empty(RARCH_PATH_CONTENT))
      {
         /* Directory and file name, as loaders pass content. */
         const char *content = path_get(RARCH_PATH_CONTENT);
         const char *slash   = strrchr(content, '/');
         const char *name    = slash ? slash + 1 : content;
         exec_arg(content, (size_t)(name - content), &used);
         exec_arg(name, strlen(name), &used);
      }
   }
   if (!filestream_read_file(exec_core, &exec_image, &exec_len))
   {
      RARCH_ERR("[Gekko] Could not read \"%s\".\n", exec_core);
      exec_image = NULL;
   }
}
#endif

static void frontend_gekko_deinit(void *data)
{
   (void)data;
#ifndef IS_SALAMANDER
#ifdef HW_RVL
   if (watcher)
   {
      watch_quit = 1;
      gk_futex_wake(&watch_quit, 1);
      gk_thread_join(watcher);
      watcher = NULL;
   }
#endif
   exec_prepare();
   unmount_all();
#endif
   /* Salamander still reads the core it runs. */
}

/* Where RetroArch lives: the directory of the program the loader
 * started, else retroarch on the first volume mounted. */
/* A loader's path on this program's volumes: some (WiiFlow) number
 * the USB drive, "usb1:". */
static void loader_path(char *s, size_t len, const char *path)
{
   const char *p = path;
   if (!strncmp(p, "usb", 3))
   {
      for (p += 3; *p >= '0' && *p <= '9'; p++)
         ;
      if (*p == ':' && p > path + 3)
      {
         size_t _len = strlcpy(s, "usb", len);
         if (_len < len)
            strlcpy(s + _len, p, len - _len);
         return;
      }
   }
   strlcpy(s, path, len);
}

static void base_dir(int argc, char *argv[], char *s, size_t len)
{
   unsigned i;
   if (argc >= 1 && argv && argv[0] && strstr(argv[0], ":/"))
   {
      char *slash;
      loader_path(s, len, argv[0]);
      if ((slash = strrchr(s, '/')))
         *slash = '\0';
      return;
   }
   for (i = 0; i < VOL_COUNT; i++)
      if (volumes[i].mounted)
      {
         snprintf(s, len, "%s:/retroarch", volumes[i].name);
         return;
      }
   strlcpy(s, "sd:/retroarch", len);
}

static void frontend_gekko_get_env(int *argc, char *argv[],
      void *args, void *params_data)
{
#ifndef IS_SALAMANDER
   struct rarch_main_wrap *params = (struct rarch_main_wrap*)params_data;
#endif
   (void)args;

   base_dir(*argc, argv, g_defaults.dirs[DEFAULT_DIR_CORE],
         sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   chdir(g_defaults.dirs[DEFAULT_DIR_CORE]);

#ifndef IS_SALAMANDER
#ifdef HAVE_LOGGER
   logger_init();
#endif

   /* Some loaders pass nothing at all; RetroArch's argument parsing
    * wants something to look at. */
   if (*argc <= 0 || !argv)
   {
      if (params)
      {
         params->content_path  = NULL;
         params->sram_path     = NULL;
         params->state_path    = NULL;
         params->config_path   = NULL;
         params->libretro_path = NULL;
         params->flags        &= ~(RARCH_MAIN_WRAP_FLAG_VERBOSE
                                 | RARCH_MAIN_WRAP_FLAG_NO_CONTENT);
         params->flags        |=   RARCH_MAIN_WRAP_FLAG_TOUCHED;
      }
   }
   else if (*argc > 2 && argv[1] && *argv[1] && argv[2] && *argv[2]
         && *argv[1] != '-' && params)
   {
      /* Loaders that start content pass its directory and file name
       * as two arguments; options (netplay's) are RetroArch's to
       * parse. */
      static char path[PATH_MAX_LENGTH];
      char dir[PATH_MAX_LENGTH];
      loader_path(dir, sizeof(dir), argv[1]);
      fill_pathname_join(path, dir, argv[2], sizeof(path));
      params->content_path  = path;
      params->sram_path     = NULL;
      params->state_path    = NULL;
      params->config_path   = NULL;
      params->libretro_path = NULL;
      params->flags        &= ~(RARCH_MAIN_WRAP_FLAG_VERBOSE
                              | RARCH_MAIN_WRAP_FLAG_NO_CONTENT);
      params->flags        |=   RARCH_MAIN_WRAP_FLAG_TOUCHED;
   }
#endif

   /* The port directory: retroarch at the root of the volume the
    * program is on. */
   {
      char volume[16];
      size_t _len = strcspn(g_defaults.dirs[DEFAULT_DIR_CORE], "/") + 1;
      strlcpy(volume, g_defaults.dirs[DEFAULT_DIR_CORE],
            _len < sizeof(volume) ? _len : sizeof(volume));
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PORT], volume,
            "retroarch", sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   }

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
         g_defaults.dirs[DEFAULT_DIR_CORE], "info",
         sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG],
         g_defaults.dirs[DEFAULT_DIR_CORE], "autoconfig",
         sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY],
         g_defaults.dirs[DEFAULT_DIR_CORE], "overlays",
         sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY],
         g_defaults.dirs[DEFAULT_DIR_CORE], "overlays/keyboards",
         sizeof(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
         g_defaults.dirs[DEFAULT_DIR_CORE], "filters/video",
         sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
         g_defaults.dirs[DEFAULT_DIR_CORE], "filters/audio",
         sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS],
         g_defaults.dirs[DEFAULT_DIR_CORE], "assets",
         sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS],
         g_defaults.dirs[DEFAULT_DIR_CORE], "cheats",
         sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.path_config,
         g_defaults.dirs[DEFAULT_DIR_CORE], "retroarch.cfg",
         sizeof(g_defaults.path_config));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM],
         g_defaults.dirs[DEFAULT_DIR_PORT], "system",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM],
         g_defaults.dirs[DEFAULT_DIR_PORT], "savefiles",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE],
         g_defaults.dirs[DEFAULT_DIR_PORT], "savestates",
         sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST],
         g_defaults.dirs[DEFAULT_DIR_PORT], "playlists",
         sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS],
         g_defaults.dirs[DEFAULT_DIR_PORT], "logs",
         sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
   /* REMAP lives under MENU_CONFIG, so that comes first. */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
         g_defaults.dirs[DEFAULT_DIR_PORT], "config",
         sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP],
         g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], "remaps",
         sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));

#ifdef IS_SALAMANDER
   /* Content a loader passed goes on to the core. */
   if (*argc > 2 && argv && argv[1] && *argv[1] && argv[2] && *argv[2])
   {
      loader_path(content_dir, sizeof(content_dir), argv[1]);
      strlcpy(content_file, argv[2], sizeof(content_file));
   }
#else
   dir_check_defaults("custom.ini");
#endif
}

#ifndef IS_SALAMANDER
static void frontend_gekko_exitspawn(char *s, size_t len, char *args)
{
   int ret;
   (void)s;
   (void)len;
   (void)args;
   if (!exec_image)
      return;
   ret = gk_exec(exec_image, (size_t)exec_len, exec_argc, exec_argv);
   RARCH_ERR("[Gekko] Could not run \"%s\" (%d).\n", exec_core, ret);
   free(exec_image);
   exec_image = NULL;
}

static bool frontend_gekko_set_fork(enum frontend_fork mode)
{
   switch (mode)
   {
      case FRONTEND_FORK_CORE:
      case FRONTEND_FORK_CORE_WITH_ARGS:
      case FRONTEND_FORK_RESTART:
         fork_mode = mode;
         return true;
      default:
         break;
   }
   return false;
}
#endif

static void frontend_gekko_process_args(int *argc, char *argv[])
{
#ifndef IS_SALAMANDER
   /* The core is the one linked into the program the loader ran. */
   if (*argc >= 1 && argv && argv[0])
   {
      const char *last_slash = strrchr(argv[0], '/');
      if (last_slash && path_is_valid(last_slash + 1))
         path_set(RARCH_PATH_CORE, last_slash + 1);
   }
#endif
}

static enum frontend_architecture frontend_gekko_get_arch(void)
{
   return FRONTEND_ARCH_PPC;
}

static int frontend_gekko_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list            = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content
      ? MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR
      : MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
   unsigned i;
   for (i = 0; i < VOL_COUNT; i++)
   {
      char root[16];
      if (!volumes[i].mounted)
         continue;
      snprintf(root, sizeof(root), "%s:/", volumes[i].name);
      menu_entries_append(list, root,
            msg_hash_to_str(MSG_EXTERNAL_APPLICATION_DIR),
            enum_idx, FILE_TYPE_DIRECTORY, 0, 0, NULL);
   }
#endif
   return 0;
}

static void frontend_gekko_shutdown(bool unused)
{
   (void)unused;
#ifndef IS_SALAMANDER
   /* exit() goes back to the loader, or the system menu. */
   exit(0);
#endif
}

frontend_ctx_driver_t frontend_ctx_gx = {
   frontend_gekko_get_env,          /* get_env */
   frontend_gekko_init,             /* init */
   frontend_gekko_deinit,           /* deinit */
   frontend_gekko_exitspawn,        /* exitspawn */
   frontend_gekko_process_args,     /* process_args */
   NULL,                            /* exec */
#ifndef IS_SALAMANDER
   frontend_gekko_set_fork,         /* set_fork */
#else
   NULL,                            /* set_fork */
#endif
   frontend_gekko_shutdown,         /* shutdown */
   NULL,                            /* get_name */
   NULL,                            /* get_os */
   NULL,                            /* load_content */
   frontend_gekko_get_arch,         /* get_architecture */
   NULL,                            /* get_powerstate */
   frontend_gekko_parse_drive_list, /* parse_drive_list */
   NULL,                            /* install_signal_handler */
   NULL,                            /* get_sighandler_state */
   NULL,                            /* set_sighandler_state */
   NULL,                            /* destroy_signal_handler_state */
   NULL,                            /* attach_console */
   NULL,                            /* detach_console */
   NULL,                            /* get_lakka_version */
   NULL,                            /* set_screen_brightness */
   NULL,                            /* set_sustained_performance_mode */
   NULL,                            /* get_cpu_model_name */
   NULL,                            /* get_user_language */
   NULL,                            /* is_narrator_running */
   NULL,                            /* accessibility_speak */
   NULL,                            /* set_gamemode */
   NULL,                            /* get_display_type */
   "gx",                            /* ident */
   NULL                             /* get_video_driver */
};

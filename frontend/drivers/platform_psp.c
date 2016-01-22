/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2016 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef VITA
#include <psp2/moduleinfo.h>
#include <psp2/power.h>

int scePowerSetArmClockFrequency(int freq);
#else
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspfpu.h>
#include <psppower.h>
#include <pspsdk.h>
#endif

#include <boolean.h>
#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <file/file_list.h>
#endif

#include "../frontend_driver.h"
#include "../../defaults.h"
#include "../../defines/psp_defines.h"
#include "../../verbosity.h"

#if defined(HAVE_KERNEL_PRX) || defined(IS_SALAMANDER)
#ifndef VITA
#include "../../bootstrap/psp1/kernel_functions.h"
#endif
#endif

#ifdef VITA
PSP2_MODULE_INFO(0, 0, "RetroArch");
int _newlib_heap_size_user = 64 * 1024 * 1024;
#else
PSP_MODULE_INFO("RetroArch", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER|THREAD_ATTR_VFPU);
#ifdef BIG_STACK
PSP_MAIN_THREAD_STACK_SIZE_KB(4*1024);
#endif
PSP_HEAP_SIZE_MAX();
#endif

char eboot_path[512];

static bool exit_spawn = false;
static bool exitspawn_start_game = false;

#ifdef IS_SALAMANDER
#include "../../file_ext.h"
#endif

static void frontend_psp_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   struct rarch_main_wrap *params = NULL;

   (void)args;

#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
#ifndef VITA
   retro_main_log_file_init("ms0:/retroarch-log.txt");
#endif
#endif
#endif

#ifdef VITA
   strlcpy(eboot_path, "cache0:/retroarch/", sizeof(eboot_path));
   strlcpy(g_defaults.dir.port, eboot_path, sizeof(g_defaults.dir.port));
#else
   strlcpy(eboot_path, argv[0], sizeof(eboot_path));
   fill_pathname_basedir(g_defaults.dir.port, argv[0], sizeof(g_defaults.dir.port));
#endif
   RARCH_LOG("port dir: [%s]\n", g_defaults.dir.port);

   fill_pathname_join(g_defaults.dir.core_assets, g_defaults.dir.port,
         "downloads", sizeof(g_defaults.dir.core_assets));
   fill_pathname_join(g_defaults.dir.assets, g_defaults.dir.port,
         "media", sizeof(g_defaults.dir.assets));
   fill_pathname_join(g_defaults.dir.core, g_defaults.dir.port,
         "cores", sizeof(g_defaults.dir.core));
   fill_pathname_join(g_defaults.dir.core_info, g_defaults.dir.port,
         "cores", sizeof(g_defaults.dir.core_info));
   fill_pathname_join(g_defaults.dir.savestate, g_defaults.dir.core,
         "savestates", sizeof(g_defaults.dir.savestate));
   fill_pathname_join(g_defaults.dir.sram, g_defaults.dir.core,
         "savefiles", sizeof(g_defaults.dir.sram));
   fill_pathname_join(g_defaults.dir.system, g_defaults.dir.core,
         "system", sizeof(g_defaults.dir.system));
   fill_pathname_join(g_defaults.dir.playlist, g_defaults.dir.core,
         "playlists", sizeof(g_defaults.dir.playlist));
   fill_pathname_join(g_defaults.path.config, g_defaults.dir.port,
         "retroarch.cfg", sizeof(g_defaults.path.config));
   fill_pathname_join(g_defaults.dir.cheats, g_defaults.dir.port,
         "cheats", sizeof(g_defaults.dir.cheats));
   fill_pathname_join(g_defaults.dir.remap, g_defaults.dir.port,
         "remaps", sizeof(g_defaults.dir.remap));

#ifdef VITA
   fill_pathname_join(g_defaults.dir.overlay, g_defaults.dir.core,
         "overlays", sizeof(g_defaults.dir.overlay));
#endif

#ifdef VITA
   params = (struct rarch_main_wrap*)params_data;
   params->verbose = true;
#endif

#ifndef IS_SALAMANDER
   if (!string_is_empty(argv[1]))
   {
      static char path[PATH_MAX_LENGTH];
      struct rarch_main_wrap *args = NULL;

      *path = '\0';
      args = (struct rarch_main_wrap*)params_data;

      if (args)
      {
         strlcpy(path, argv[1], sizeof(path));

         args->touched        = true;
         args->no_content     = false;
         args->verbose        = false;
         args->config_path    = NULL;
         args->sram_path      = NULL;
         args->state_path     = NULL;
         args->content_path   = path;
         args->libretro_path  = NULL;

         RARCH_LOG("argv[0]: %s\n", argv[0]);
         RARCH_LOG("argv[1]: %s\n", argv[1]);
         RARCH_LOG("argv[2]: %s\n", argv[2]);

         RARCH_LOG("Auto-start game %s.\n", argv[1]);
      }
   }
#endif
}

static void frontend_psp_deinit(void *data)
{
   (void)data;
#ifndef IS_SALAMANDER
   bool *verbose      = retro_main_verbosity();
   *verbose           = false;

#ifdef HAVE_FILE_LOGGER
   event_cmd_ctl(EVENT_CMD_LOG_FILE_DEINIT, NULL);
#endif

#endif
}

static void frontend_psp_shutdown(bool unused)
{
   (void)unused;
#ifdef VITA
   return;
#else
   sceKernelExitGame();
#endif
}

#ifndef VITA
static int exit_callback(int arg1, int arg2, void *common)
{
   frontend_psp_deinit(NULL);
   frontend_psp_shutdown(false);
   return 0;
}

static int callback_thread(SceSize args, void *argp)
{
   int cbid = sceKernelCreateCallback("Exit callback", exit_callback, NULL);
   sceKernelRegisterExitCallback(cbid);
   sceKernelSleepThreadCB();

   return 0;
}

static int setup_callback(void)
{
   int thread_id = sceKernelCreateThread("update_thread",
         callback_thread, 0x11, 0xFA0, 0, 0);

   if (thread_id >= 0)
      sceKernelStartThread(thread_id, 0, 0);

   return thread_id;
}
#endif

static void frontend_psp_init(void *data)
{
#ifndef IS_SALAMANDER
#ifndef VITA
   (void)data;

   /* TODO/FIXME - Err on the safe side for now and
    * assume these aren't there with the PSP2/Vita SDKs.
    */

   /* initialize debug screen */
   pspDebugScreenInit();
   pspDebugScreenClear();

   setup_callback();

   pspFpuSetEnable(0); /* disable FPU exceptions */
   scePowerSetClockFrequency(333,333,166);
#else
   scePowerSetArmClockFrequency(444);
#endif
#endif

#if defined(HAVE_KERNEL_PRX) || defined(IS_SALAMANDER)
#ifndef VITA
   pspSdkLoadStartModule("kernel_functions.prx", PSP_MEMORY_PARTITION_KERNEL);
#endif
#endif
}

static void frontend_psp_exec(const char *path, bool should_load_game)
{
#if defined(HAVE_KERNEL_PRX) || defined(IS_SALAMANDER)
   char argp[512] = {0};
   SceSize   args = 0;

   argp[0] = '\0';
   strlcpy(argp, eboot_path, sizeof(argp));
   args = strlen(argp) + 1;

#ifndef IS_SALAMANDER
   char *fullpath = NULL;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   if (should_load_game && !string_is_empty(fullpath))
   {
      argp[args] = '\0';
      strlcat(argp + args, fullpath, sizeof(argp) - args);
      args += strlen(argp + args) + 1;
   }
#endif

   RARCH_LOG("Attempt to load executable: [%s].\n", path);

   exitspawn_kernel(path, args, argp);

#endif
}

static void frontend_psp_set_fork(bool exit, bool start_game, bool restart)
{
   exit_spawn = true;
   exitspawn_start_game = start_game;
}

static void frontend_psp_exitspawn(char *s, size_t len)
{
   bool should_load_game = false;
#ifndef IS_SALAMANDER
   should_load_game = exitspawn_start_game;

   if (!exit_spawn)
      return;
#endif
   frontend_psp_exec(s, should_load_game);
}

static int frontend_psp_get_rating(void)
{
#ifdef VITA
   return 6; /* Go with a conservative figure for now. */
#else
   return 4;
#endif
}

static enum frontend_powerstate frontend_psp_get_powerstate(int *seconds, int *percent)
{
   enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;
#ifndef VITA
   int battery                  = scePowerIsBatteryExist();
#endif
   int plugged                  = scePowerIsPowerOnline();
   int charging                 = scePowerIsBatteryCharging();

   *percent = scePowerGetBatteryLifePercent();
   *seconds = scePowerGetBatteryLifeTime() * 60;

#ifndef VITA
   if (!battery)
   {
      ret = FRONTEND_POWERSTATE_NO_SOURCE;
      *seconds = -1;
      *percent = -1;
   }
   else
#endif
   if (charging)
      ret = FRONTEND_POWERSTATE_CHARGING;
   else if (plugged)
      ret = FRONTEND_POWERSTATE_CHARGED;
   else
      ret = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   return ret;
}

enum frontend_architecture frontend_psp_get_architecture(void)
{
#ifdef VITA
   return FRONTEND_ARCH_ARM;
#else
   return FRONTEND_ARCH_MIPS;
#endif
}

static int frontend_psp_parse_drive_list(void *data)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t*)data;

#ifdef VITA
   menu_entries_push(list,
         "cache0:/", "", MENU_FILE_DIRECTORY, 0, 0);
#else
   menu_entries_push(list,
         "ms0:/", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "ef0:/", "", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         "host0:/", "", MENU_FILE_DIRECTORY, 0, 0);
#endif
#endif

   return 0;
}

frontend_ctx_driver_t frontend_ctx_psp = {
   frontend_psp_get_environment_settings,
   frontend_psp_init,
   frontend_psp_deinit,
   frontend_psp_exitspawn,
   NULL,                         /* process_args */
   frontend_psp_exec,
   frontend_psp_set_fork,
   frontend_psp_shutdown,
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_psp_get_rating,
   NULL,                         /* load_content */
   frontend_psp_get_architecture,
   frontend_psp_get_powerstate,
   frontend_psp_parse_drive_list,
#ifdef VITA
   "vita",
#else
   "psp",
#endif
};

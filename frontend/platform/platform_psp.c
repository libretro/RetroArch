/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2014 - Daniel De Matteis
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

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspfpu.h>
#include <psppower.h>
#include <pspsdk.h>

#include <stdint.h>
#include "../../boolean.h"
#include <stddef.h>
#include <string.h>

#include "../../psp/sdk_defines.h"
#include "../../file.h"

#if defined(HAVE_KERNEL_PRX) || defined(IS_SALAMANDER)
#include "../../psp1/kernel_functions.h"
#endif

PSP_MODULE_INFO("RetroArch PSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER|THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_MAX();

char eboot_path[512];

#ifdef IS_SALAMANDER
#include "../../file_ext.h"
#endif

static void frontend_psp_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   (void)args;
#ifndef IS_SALAMANDER
   g_extern.verbose = true;

#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   g_extern.log_file = fopen("ms0:/retroarch-log.txt", "w");
#endif
#endif

   strlcpy(eboot_path, argv[0], sizeof(eboot_path));

   fill_pathname_basedir(g_defaults.port_dir, argv[0], sizeof(g_defaults.port_dir));
   RARCH_LOG("port dir: [%s]\n", g_defaults.port_dir);

   fill_pathname_join(g_defaults.assets_dir, g_defaults.port_dir, "media", sizeof(g_defaults.assets_dir));
   fill_pathname_join(g_defaults.core_dir, g_defaults.port_dir, "cores", sizeof(g_defaults.core_dir));
   fill_pathname_join(g_defaults.core_info_dir, g_defaults.port_dir, "cores", sizeof(g_defaults.core_info_dir));
   fill_pathname_join(g_defaults.savestate_dir, g_defaults.core_dir, "savestates", sizeof(g_defaults.savestate_dir));
   fill_pathname_join(g_defaults.sram_dir, g_defaults.core_dir, "savefiles", sizeof(g_defaults.sram_dir));
   fill_pathname_join(g_defaults.system_dir, g_defaults.core_dir, "system", sizeof(g_defaults.system_dir));
   fill_pathname_join(g_defaults.config_path, g_defaults.port_dir, "retroarch.cfg", sizeof(g_defaults.config_path));

   if (argv[1] && (argv[1][0] != '\0'))
   {
      static char path[PATH_MAX];
      *path = '\0';
      struct rarch_main_wrap *args = (struct rarch_main_wrap*)params_data;

      if (args)
      {
         strlcpy(path, argv[1], sizeof(path));

         args->touched        = true;
         args->no_rom         = false;
         args->verbose        = false;
         args->config_path    = NULL;
         args->sram_path      = NULL;
         args->state_path     = NULL;
         args->rom_path       = path;
         args->libretro_path  = NULL;

         RARCH_LOG("argv[0]: %s\n", argv[0]);
         RARCH_LOG("argv[1]: %s\n", argv[1]);
         RARCH_LOG("argv[2]: %s\n", argv[2]);

         RARCH_LOG("Auto-start game %s.\n", argv[1]);
      }
   }
}

static void frontend_psp_deinit(void *data)
{
   (void)data;
#ifndef IS_SALAMANDER
   g_extern.verbose = false;

#ifdef HAVE_FILE_LOGGER
   if (g_extern.log_file)
      fclose(g_extern.log_file);
   g_extern.log_file = NULL;
#endif

#endif
}

static void frontend_psp_shutdown(bool unused)
{
   (void)unused;
   sceKernelExitGame();
}

static int exit_callback(int arg1, int arg2, void *common)
{
   frontend_psp_deinit(NULL);
   frontend_psp_shutdown(false);
   return 0;
}

int callback_thread(SceSize args, void *argp)
{
   int cbid = sceKernelCreateCallback("Exit callback", exit_callback, NULL);
   sceKernelRegisterExitCallback(cbid);
   sceKernelSleepThreadCB();

   return 0;
}

static int setup_callback(void)
{
   int thread_id = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);

   if (thread_id >= 0)
      sceKernelStartThread(thread_id, 0, 0);

   return thread_id;
}

static void frontend_psp_init(void *data)
{
#ifndef IS_SALAMANDER
   (void)data;
   //initialize debug screen
   pspDebugScreenInit(); 
   pspDebugScreenClear();
   
   setup_callback();
   
   pspFpuSetEnable(0);//disable FPU exceptions
   scePowerSetClockFrequency(333,333,166);
#endif

#if defined(HAVE_KERNEL_PRX) || defined(IS_SALAMANDER)
   pspSdkLoadStartModule("kernel_functions.prx", PSP_MEMORY_PARTITION_KERNEL);
#endif
}



static void frontend_psp_exec(const char *path, bool should_load_game)
{
#if defined(HAVE_KERNEL_PRX) || defined(IS_SALAMANDER)

   char argp[512];
   SceSize args = 0;

   argp[0] = '\0';
   strlcpy(argp, eboot_path, sizeof(argp));
   args = strlen(argp) + 1;

#ifndef IS_SALAMANDER
   if (should_load_game && g_extern.fullpath[0] != '\0')
   {
      argp[args] = '\0';
      strlcat(argp + args, g_extern.fullpath, sizeof(argp) - args);
      args += strlen(argp + args) + 1;
   }
#endif

   RARCH_LOG("Attempt to load executable: [%s].\n", path);

   exitspawn_kernel(path, args, argp);

#endif
}


static void frontend_psp_exitspawn(char *core_path, size_t sizeof_core_path)
{
   bool should_load_game = false;
#ifndef IS_SALAMANDER
   if (g_extern.lifecycle_state & (1ULL << MODE_EXITSPAWN_START_GAME))
      should_load_game = true;

#endif
   frontend_psp_exec(core_path, should_load_game);
}

static int frontend_psp_get_rating(void)
{
   return 4;
}

const frontend_ctx_driver_t frontend_ctx_psp = {
   frontend_psp_get_environment_settings, /* get_environment_settings */
   frontend_psp_init,            /* init */
   frontend_psp_deinit,          /* deinit */
   frontend_psp_exitspawn,       /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* process_events */
   frontend_psp_exec,            /* exec */
   frontend_psp_shutdown,        /* shutdown */
   NULL,                         /* get_name */
   frontend_psp_get_rating,      /* get_rating */
   "psp",
};

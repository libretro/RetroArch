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

PSP_MODULE_INFO("RetroArch PSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER|THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_MAX();

static int exit_callback(int arg1, int arg2, void *common)
{
   g_extern.verbose = false;

#ifdef HAVE_FILE_LOGGER
   if (g_extern.log_file)
      fclose(g_extern.log_file);
   g_extern.log_file = NULL;
#endif

   sceKernelExitGame();
   return 0;
}

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

   fill_pathname_basedir(default_paths.port_dir, argv[0], sizeof(default_paths.port_dir));
   RARCH_LOG("port dir: [%s]\n", default_paths.port_dir);

   fill_pathname_join(default_paths.assets_dir, default_paths.port_dir, "media", sizeof(default_paths.assets_dir));
   fill_pathname_join(default_paths.core_dir, default_paths.port_dir, "cores", sizeof(default_paths.core_dir));
   fill_pathname_join(default_paths.core_info_dir, default_paths.port_dir, "cores", sizeof(default_paths.core_info_dir));
   fill_pathname_join(default_paths.savestate_dir, default_paths.core_dir, "savestates", sizeof(default_paths.savestate_dir));
   fill_pathname_join(default_paths.sram_dir, default_paths.core_dir, "savefiles", sizeof(default_paths.sram_dir));
   fill_pathname_join(default_paths.system_dir, default_paths.core_dir, "system", sizeof(default_paths.system_dir));
   fill_pathname_join(default_paths.config_path, default_paths.port_dir, "retroarch.cfg", sizeof(default_paths.config_path));

   if (argv[1] && (argv[1][0] != '\0'))
   {
      char path[PATH_MAX];
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
         args->rom_path       = strdup(path);
         args->libretro_path  = NULL;

         RARCH_LOG("argv[0]: %s\n", argv[0]);
         RARCH_LOG("argv[1]: %s\n", argv[1]);
         RARCH_LOG("argv[2]: %s\n", argv[2]);

         RARCH_LOG("Auto-start game %s.\n", argv[1]);
      }
   }
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
   (void)data;
   //initialize debug screen
   pspDebugScreenInit(); 
   pspDebugScreenClear();
   
   setup_callback();
   
   pspFpuSetEnable(0);//disable FPU exceptions
   scePowerSetClockFrequency(333,333,166);

   pspSdkLoadStartModule("kernelFunctions.prx", PSP_MEMORY_PARTITION_KERNEL);
}

static void frontend_psp_deinit(void *data)
{
   (void)data;
   sceKernelExitGame();
}

static int frontend_psp_get_rating(void)
{
   return 4;
}

const frontend_ctx_driver_t frontend_ctx_psp = {
   frontend_psp_get_environment_settings, /* get_environment_settings */
   frontend_psp_init,            /* init */
   frontend_psp_deinit,          /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* process_events */
   NULL,                  	      /* exec */
   NULL,                         /* shutdown */
   frontend_psp_get_rating,      /* get_rating */
   "psp",
#ifdef IS_SALAMANDER
   NULL,
#endif
};

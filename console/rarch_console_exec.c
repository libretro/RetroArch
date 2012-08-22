/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include <stdio.h>

#if defined(__CELLOS_LV2__)
#include <cell/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil_common.h>
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>
#elif defined(_XBOX)
#include <xtl.h>
#elif defined(GEKKO)
#include <fat.h>
#include <ogc/lwp_threads.h>
#include <ogc/system.h>
#include <gctypes.h>
#ifdef HW_RVL
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>
#endif
#include "exec/dol.h"
#endif

#include "rarch_console_exec.h"
#include "../retroarch_logger.h"

void rarch_console_exec(const char *path)
{
   RARCH_LOG("Attempt to load executable: [%s].\n", path);
#if defined(_XBOX)
   XLaunchNewImage(path, NULL);
#elif defined(__CELLOS_LV2__)
   char spawn_data[256];
   for(unsigned int i = 0; i < sizeof(spawn_data); ++i)
      spawn_data[i] = i & 0xff;

   char spawn_data_size[16];
   snprintf(spawn_data_size, sizeof(spawn_data_size), "%d", 256);

   const char * const spawn_argv[] = {
      spawn_data_size,
      "test argv for",
      "sceNpDrmProcessExitSpawn2()",
      NULL
   };

   SceNpDrmKey * k_licensee = NULL;
   int ret = sceNpDrmProcessExitSpawn2(k_licensee, path, (const char** const)spawn_argv, NULL, (sys_addr_t)spawn_data, 256, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);

   if(ret <  0)
   {
      RARCH_WARN("SELF file is not of NPDRM type, trying another approach to boot it...\n");
      sys_game_process_exitspawn(path, NULL, NULL, NULL, 0, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
   }

   sceNpTerm();
   sys_net_finalize_network();
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
#elif defined(GEKKO)
   FILE * fp = fopen(path, "rb");
   if (fp == NULL)
   {
      RARCH_ERR("Could not execute DOL file.\n");
      return;
   }
   fseek(fp, 0, SEEK_END);
   size_t size = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   uint8_t *mem = (uint8_t *)0x92000000; // should be safe for this small program to use
   fread(mem, 1, size, fp);
   fclose(fp);
#ifdef HW_RVL
   fatUnmount("sd:");
   fatUnmount("usb:");
#endif
   fatUnmount("carda:");
   fatUnmount("cardb:");
#ifdef HW_RVL
   __io_wiisd.shutdown();
   __io_usbstorage.shutdown();
#endif
   uint32_t *ep = load_dol_image(mem);
   
   if (ep[1] == ARGV_MAGIC)
      dol_copy_argv((struct __argv *) &ep[2]);

   RARCH_LOG("jumping to 0x%08X\n", (uint32_t) ep);

   SYS_ResetSystem(SYS_SHUTDOWN,0,0);

   __lwp_thread_stopmultitasking((void(*)()) ep);
#else
   RARCH_WARN("External loading of executables is not supported for this platform.\n");
#endif
}

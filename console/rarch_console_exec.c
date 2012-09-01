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
#include <string.h>
#include <fat.h>
#include <gctypes.h>
#include <ogc/cache.h>
#include <ogc/lwp_threads.h>
#include <ogc/system.h>

#ifdef HW_RVL
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#else
#include <ogc/aram.h>
#endif

#endif

#include "rarch_console_exec.h"
#include "../retroarch_logger.h"

#ifdef GEKKO

#ifdef HW_RVL

#define EXECUTE_ADDR ((uint8_t *) 0x91800000)
#define BOOTER_ADDR ((uint8_t *) 0x93000000)
#define ARGS_ADDR ((uint8_t *) 0x93200000)

extern uint8_t _binary_gx_app_booter_app_booter_wii_bin_start[];
extern uint8_t _binary_gx_app_booter_app_booter_wii_bin_end[];
#define booter_start _binary_gx_app_booter_app_booter_wii_bin_start
#define booter_end _binary_gx_app_booter_app_booter_wii_bin_end

#else

#define ARAMSTART 0x8000
#define BOOTER_ADDR ((uint8_t *) 0x81300000)
extern void __exception_closeall(void);

extern uint8_t _binary_gx_app_booter_app_booter_ngc_bin_start[];
extern uint8_t _binary_gx_app_booter_app_booter_ngc_bin_end[];
#define booter_start _binary_gx_app_booter_app_booter_ngc_bin_start
#define booter_end _binary_gx_app_booter_app_booter_ngc_bin_end

#endif

#ifdef HW_RVL
// NOTE: this does not update the path to point to the new loading .dol file.
// we only need it for keeping the current directory anyway.
void dol_copy_argv_path(void)
{
   struct __argv *argv = (struct __argv *) ARGS_ADDR;
   memset(ARGS_ADDR, 0, sizeof(struct __argv));
   char *cmdline = (char *) ARGS_ADDR + sizeof(struct __argv);
   argv->argvMagic = ARGV_MAGIC;
   argv->commandLine = cmdline;
   size_t len = strlen(__system_argv->argv[0]);
   memcpy(cmdline, __system_argv->argv[0], ++len);
   cmdline[len++] = 0;
   cmdline[len++] = 0;
   argv->length = len;
   DCFlushRange(ARGS_ADDR, sizeof(struct __argv) + argv->length);
}
#endif
#endif

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

#ifdef HW_RVL
   fseek(fp, 0, SEEK_END);
   size_t size = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   fread(EXECUTE_ADDR, 1, size, fp);
   fclose(fp);
   DCFlushRange(EXECUTE_ADDR, size);
#else
   uint8_t buffer[0x800];
   size_t size;
   size_t offset = 0;

   AR_Init(NULL, 0);
   while ((size = fread(buffer, 1, sizeof(buffer), fp)) != 0)
   {
      if (size != sizeof(buffer))
         memset(&buffer[size], 0, sizeof(buffer) - size);

      AR_StartDMA(AR_MRAMTOARAM, (u32) buffer, (u32) offset + ARAMSTART, sizeof(buffer));
      while (AR_GetDMAStatus());
      offset += sizeof(buffer);
   }
#endif

#ifdef HW_RVL
   dol_copy_argv_path();
#endif

   fatUnmount("carda:");
   fatUnmount("cardb:");
#ifdef HW_RVL
   fatUnmount("sd:");
   fatUnmount("usb:");
   __io_wiisd.shutdown();
   __io_usbstorage.shutdown();
#endif

   size_t booter_size = booter_end - booter_start;
   memcpy(BOOTER_ADDR, booter_start, booter_size);
   DCFlushRange(BOOTER_ADDR, booter_size);

   RARCH_LOG("jumping to %08x\n", (unsigned) BOOTER_ADDR);
#ifdef HW_RVL
   SYS_ResetSystem(SYS_SHUTDOWN,0,0);
#else // we need to keep the ARAM alive for the booter app.
   int level;
   _CPU_ISR_Disable(level);
   __exception_closeall();
#endif
   __lwp_thread_stopmultitasking((void (*)(void)) BOOTER_ADDR);
#ifdef HW_DOL
   _CPU_ISR_Restore(level);
#endif
#else
   RARCH_WARN("External loading of executables is not supported for this platform.\n");
#endif
}

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <cell/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil_common.h>
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>

#include "../../retroarch_logger.h"

static void rarch_console_exec(const char *path)
{
   RARCH_LOG("Attempt to load executable: [%s].\n", path);
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
}

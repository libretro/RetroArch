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
#include <stdlib.h>
#include <string.h>
#include "../boolean.h"
#include "../compat/strl.h"
#include "../libretro.h"
#include "../general.h"
#include "../compat/strl.h"
#include "../file.h"

#include "rarch_console.h"

default_paths_t default_paths;

#ifdef HAVE_RARCH_EXEC

#ifdef __CELLOS_LV2__
#include <cell/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil_common.h>
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>
#endif

void rarch_exec (void)
{
   if(g_console.return_to_launcher)
   {
      RARCH_LOG("Attempt to load executable: [%s].\n", g_console.launch_app_on_exit);
#if defined(_XBOX)
      XLaunchNewImage(g_console.launch_app_on_exit, NULL);
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
      int ret = sceNpDrmProcessExitSpawn2(k_licensee, g_console.launch_app_on_exit, (const char** const)spawn_argv, NULL, (sys_addr_t)spawn_data, 256, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
      if(ret <  0)
      {
         RARCH_WARN("SELF file is not of NPDRM type, trying another approach to boot it...\n");
	 sys_game_process_exitspawn(g_console.launch_app_on_exit, NULL, NULL, NULL, 0, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
      }
      sceNpTerm();
      cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);
      cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
#endif
   }
}

#endif

#ifdef HAVE_RSOUND
bool rarch_console_rsound_start(const char *ip)
{
   strlcpy(g_settings.audio.driver, "rsound", sizeof(g_settings.audio.driver));
   strlcpy(g_settings.audio.device, ip, sizeof(g_settings.audio.device));
   driver.audio_data = NULL;

   // If driver already has started, it must be reinited.
   if (driver.audio_data)
   {
      uninit_audio();
      driver.audio_data = NULL;
      init_drivers_pre();
      init_audio();
   }
   return g_extern.audio_active;
}

void rarch_console_rsound_stop(void)
{
   strlcpy(g_settings.audio.driver, config_get_default_audio(), sizeof(g_settings.audio.driver));

   // If driver already has started, it must be reinited.
   if (driver.audio_data)
   {
      uninit_audio();
      driver.audio_data = NULL;
      init_drivers_pre();
      init_audio();
   }
}
#endif

/*============================================================
  STRING HANDLING
  ============================================================ */

void rarch_convert_char_to_wchar(wchar_t *buf, const char * str, size_t size)
{
   mbstowcs(buf, str, size / sizeof(wchar_t));
}

const char * rarch_convert_wchar_to_const_char(const wchar_t * wstr)
{
   static char str[256];
   wcstombs(str, wstr, sizeof(str));
   return str;
}

void rarch_extract_directory(char *buf, const char *path, size_t size)
{
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   char *base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
      buf[0] = '\0';
}

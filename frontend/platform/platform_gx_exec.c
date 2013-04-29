/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012-2013 - Michael Lelli
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

#include <string.h>
#include <fat.h>
#include <gctypes.h>
#include <ogc/cache.h>
#include <ogc/lwp_threads.h>
#include <ogc/system.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>

#define EXECUTE_ADDR ((uint8_t *) 0x91800000)
#define BOOTER_ADDR ((uint8_t *) 0x93000000)
#define ARGS_ADDR ((uint8_t *) 0x93200000)

extern uint8_t _binary_wii_app_booter_app_booter_bin_start[];
extern uint8_t _binary_wii_app_booter_app_booter_bin_end[];
#define booter_start _binary_wii_app_booter_app_booter_bin_start
#define booter_end _binary_wii_app_booter_app_booter_bin_end

#include "../../retroarch_logger.h"

static void dol_copy_argv_path(const char *fullpath)
{
   struct __argv *argv = (struct __argv *) ARGS_ADDR;
   memset(ARGS_ADDR, 0, sizeof(struct __argv));
   char *cmdline = (char *) ARGS_ADDR + sizeof(struct __argv);
   argv->argvMagic = ARGV_MAGIC;
   argv->commandLine = cmdline;
   size_t len = strlen(__system_argv->argv[0]);
   memcpy(cmdline, __system_argv->argv[0], len);
   cmdline[len++] = 0;

#ifndef IS_SALAMANDER
   // file must be split into two parts, the path and the actual filename
   // done to be compatible with loaders
   if (fullpath && strchr(fullpath, '/') != (char *)-1)
   {
      char tmp[PATH_MAX];

      // basedir
      fill_pathname_parent_dir(tmp, fullpath, sizeof(tmp));
      size_t t_len = strlen(tmp);
      memcpy(cmdline + len, tmp, t_len);
      len += t_len;
      cmdline[len++] = 0;

      // filename
      char *name = strrchr(fullpath, '/') + 1;
      t_len = strlen(name);
      memcpy(cmdline + len, name, t_len);
      len += t_len;
      cmdline[len++] = 0;
   }
#endif
   cmdline[len++] = 0;
   argv->length = len;
   DCFlushRange(ARGS_ADDR, sizeof(struct __argv) + argv->length);
}

// WARNING: after we move any data into EXECUTE_ADDR, we can no longer use any
// heap memory and are restricted to the stack only
static void rarch_console_exec(const char *path, bool should_load_game)
{
   RARCH_LOG("Attempt to load executable: [%s].\n", path);

   FILE * fp = fopen(path, "rb");
   if (fp == NULL)
   {
      RARCH_ERR("Could not open DOL file %s.\n", path);
      return;
   }

   fseek(fp, 0, SEEK_END);
   size_t size = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   // try to allocate a buffer for it. if we can't, fail
   void *dol = malloc(size);
   if (!dol)
   {
      RARCH_ERR("Could not execute DOL file %s.\n", path);
      fclose(fp);
      return;
   }

   fread(dol, 1, size, fp);
   fclose(fp);

   fatUnmount("carda:");
   fatUnmount("cardb:");
   fatUnmount("sd:");
   fatUnmount("usb:");
   __io_wiisd.shutdown();
   __io_usbstorage.shutdown();

   // luckily for us, newlib's memmove doesn't allocate a seperate buffer for
   // copying in situations of overlap, so it's safe to do this
   memmove(EXECUTE_ADDR, dol, size);
   DCFlushRange(EXECUTE_ADDR, size);

   dol_copy_argv_path(should_load_game ? g_extern.fullpath : NULL);

   size_t booter_size = booter_end - booter_start;
   memcpy(BOOTER_ADDR, booter_start, booter_size);
   DCFlushRange(BOOTER_ADDR, booter_size);

   RARCH_LOG("jumping to %08x\n", (unsigned) BOOTER_ADDR);
   SYS_ResetSystem(SYS_SHUTDOWN,0,0);
   __lwp_thread_stopmultitasking((void (*)(void)) BOOTER_ADDR);
}

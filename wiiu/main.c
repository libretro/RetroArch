/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2016 - Ali Bouhlel
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>
#include <iosuhax.h>
#include <sys/iosupport.h>

#include "hbl.h"

#include "fs/fs_utils.h"
#include "fs/sd_fat_devoptab.h"

#include "system/dynamic.h"
#include "system/memory.h"
#include "system/exception_handler.h"

#include <wiiu/gx2.h>
#include <wiiu/ios.h>
#include <wiiu/kpad.h>
#include <wiiu/os.h>
#include <wiiu/procui.h>
#include <wiiu/sysapp.h>

/**
 * This file contains the main entrypoints for the Wii U executable that
 * set up the call to main().
 */

int main(int argc, char **argv);
void __fini(void);
void __init(void);

static void fsdev_init(void);
static void fsdev_exit(void);

static int iosuhaxMount = 0;
static int mcp_hook_fd = -1;

/* HBL elf entry point */
int __entry_menu(int argc, char **argv)
{
   int ret;

   InitFunctionPointers();
   memoryInitialize();
   __init();
   fsdev_init();

   ret = main(argc, argv);

   fsdev_exit();
   __fini();
   memoryRelease();
   return ret;
}

/* RPX entry point */
__attribute__((noreturn))
void _start(int argc, char **argv)
{
   memoryInitialize();
   __init();
   fsdev_init();
   main(argc, argv);
   fsdev_exit();

   /* TODO: fix elf2rpl so it doesn't error with "Could not find matching symbol
      for relocation" then uncomment this */
#if 0
   __fini();
#endif
   memoryRelease();
   SYSRelaunchTitle(0, 0);
   exit(0);
}

void __eabi(void)
{

}

__attribute__((weak))
void __init(void)
{
   extern void (*const __CTOR_LIST__)(void);
   extern void (*const __CTOR_END__)(void);

   void (*const *ctor)(void) = &__CTOR_LIST__;
   while (ctor < &__CTOR_END__) {
      (*ctor++)();
   }
}

__attribute__((weak))
void __fini(void)
{
   extern void (*const __DTOR_LIST__)(void);
   extern void (*const __DTOR_END__)(void);

   void (*const *dtor)(void) = &__DTOR_LIST__;
   while (dtor < &__DTOR_END__) {
      (*dtor++)();
   }
}

/* libiosuhax related */

//just to be able to call async
void someFunc(void *arg)
{
   (void)arg;
}

int MCPHookOpen(void)
{
   //take over mcp thread
   mcp_hook_fd = IOS_Open("/dev/mcp", 0);

   if (mcp_hook_fd < 0)
      return -1;

   IOS_IoctlAsync(mcp_hook_fd, 0x62, (void *)0, 0, (void *)0, 0, someFunc, (void *)0);
   //let wupserver start up
   usleep(1000);

   if (IOSUHAX_Open("/dev/mcp") < 0)
   {
      IOS_Close(mcp_hook_fd);
      mcp_hook_fd = -1;
      return -1;
   }

   return 0;
}

void MCPHookClose(void)
{
   if (mcp_hook_fd < 0)
      return;

   //close down wupserver, return control to mcp
   IOSUHAX_Close();
   //wait for mcp to return
   usleep(1000);
   IOS_Close(mcp_hook_fd);
   mcp_hook_fd = -1;
}

static void fsdev_init(void)
{
   iosuhaxMount = 0;
   int res = IOSUHAX_Open(NULL);

   if (res < 0)
      res = MCPHookOpen();

   if (res < 0)
      mount_sd_fat("sd");
   else
   {
      iosuhaxMount = 1;
      fatInitDefault();
   }
}

static void fsdev_exit(void)
{
   if (iosuhaxMount)
   {
      fatUnmount("sd:");
      fatUnmount("usb:");

      if (mcp_hook_fd >= 0)
         MCPHookClose();
      else
         IOSUHAX_Close();
   }
   else
      unmount_sd_fat("sd");
}

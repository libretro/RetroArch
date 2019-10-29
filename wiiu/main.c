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

#if defined(HAVE_IOSUHAX) && defined(HAVE_LIBFAT)
#include <fat.h>
#include <iosuhax.h>
#include <sys/iosupport.h>
#endif

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

bool iosuhaxMount = 0;

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
   __fini();
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

#ifdef HAVE_IOSUHAX
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
#endif //HAVE_IOSUHAX

static bool try_init_iosuhax(void)
{
#ifdef HAVE_IOSUHAX
   int result = IOSUHAX_Open(NULL);
   if(result < 0)
      result = MCPHookOpen();

   return (result < 0) ? false : true;
#else //don't HAVE_IOSUHAX
   return false;
#endif
}

static void try_shutdown_iosuhax(void)
{
#ifdef HAVE_IOSUHAX
   if(!iosuhaxMount)
    return;

   if (mcp_hook_fd >= 0)
    MCPHookClose();
   else
    IOSUHAX_Close();
#endif //HAVE_IOSUHAX

   iosuhaxMount = false;
}

/**
 * Mount the filesystem(s) needed by the application. By default, we
 * mount the SD card to /sd.
 *
 * The 'iosuhaxMount' symbol used here is public and can be referenced
 * in overriding implementations.
 */
__attribute__((weak))
void __mount_filesystems(void)
{
#ifdef HAVE_LIBFAT
   if(iosuhaxMount)
      fatInitDefault();
   else
      mount_sd_fat("sd");
#else
   mount_sd_fat("sd");
#endif
}

/**
 * Unmount filesystems. Implementing applications should be careful to
 * clean up anything mounted in __mount_filesystems() here.
 */
__attribute__((weak))
void __unmount_filesystems(void)
{
#ifdef HAVE_LIBFAT
   if (iosuhaxMount)
   {
      fatUnmount("sd:");
      fatUnmount("usb:");
   }
   else
      unmount_sd_fat("sd");
#else
   unmount_sd_fat("sd");
#endif
}

static void fsdev_init(void)
{
   iosuhaxMount = try_init_iosuhax();

   __mount_filesystems();
}

static void fsdev_exit(void)
{
   __unmount_filesystems();
   try_shutdown_iosuhax();
}

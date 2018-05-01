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
#include <net/net_compat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <retro_timers.h>

#include <fat.h>
#include <iosuhax.h>
#include <sys/iosupport.h>
#include <wiiu/gx2.h>
#include <wiiu/ios.h>
#include <wiiu/kpad.h>
#include <wiiu/os.h>
#include <wiiu/procui.h>
#include <wiiu/sysapp.h>

#include "main.h"

/**
 * This file contains the main entrypoints for the Wii U executable.
 */

static void fsdev_init(void);
static void fsdev_exit(void);

#define WIIU_SD_PATH "sd:/"
#define WIIU_USB_PATH "usb:/"

static int wiiu_log_socket = -1;
static volatile int wiiu_log_lock = 0;

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

void wiiu_log_init(const char *ipString, int port)
{
   wiiu_log_lock = 0;

   wiiu_log_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

   if (wiiu_log_socket < 0)
      return;

   struct sockaddr_in connect_addr;
   memset(&connect_addr, 0, sizeof(connect_addr));
   connect_addr.sin_family = AF_INET;
   connect_addr.sin_port = port;
   inet_aton(ipString, &connect_addr.sin_addr);

   if (connect(wiiu_log_socket, (struct sockaddr *)&connect_addr, sizeof(connect_addr)) < 0)
   {
      socketclose(wiiu_log_socket);
      wiiu_log_socket = -1;
   }
}

void wiiu_log_deinit(void)
{
   if (wiiu_log_socket >= 0)
   {
      socketclose(wiiu_log_socket);
      wiiu_log_socket = -1;
   }
}
static ssize_t wiiu_log_write(struct _reent *r, void *fd, const char *ptr, size_t len)
{
   if (wiiu_log_socket < 0)
      return len;

   while (wiiu_log_lock)
      OSSleepTicks(((248625000 / 4)) / 1000);

   wiiu_log_lock = 1;

   int ret;
   int remaining = len;

   while (remaining > 0)
   {
      int block = remaining < 1400 ? remaining : 1400; // take max 1400 bytes per UDP packet
      ret = send(wiiu_log_socket, ptr, block, 0);

      if (ret < 0)
         break;

      remaining -= ret;
      ptr += ret;
   }

   wiiu_log_lock = 0;

   return len;
}
void net_print(const char *str)
{
   wiiu_log_write(NULL, 0, str, strlen(str));
}

void net_print_exp(const char *str)
{
   send(wiiu_log_socket, str, strlen(str), 0);
}

#if defined(PC_DEVELOPMENT_IP_ADDRESS) && defined(PC_DEVELOPMENT_TCP_PORT)
static devoptab_t dotab_stdout =
{
   "stdout_net", // device name
   0,            // size of file structure
   NULL,         // device open
   NULL,         // device close
   wiiu_log_write,    // device write
   NULL,
   /* ... */
};
#endif

void SaveCallback(void)
{
   OSSavesDone_ReadyToRelease();
}

static bool swap_is_pending(void* start_time)
{
   uint32_t swap_count, flip_count;
   OSTime last_flip , last_vsync;

   GX2GetSwapStatus(&swap_count, &flip_count, &last_flip, &last_vsync);

   return last_vsync < *(OSTime*)start_time;
}

static void do_network_init(void)
{
#ifdef IS_SALAMANDER
   socket_lib_init();
#else
   network_init();
#endif
}

static void init_pad_libraries(void)
{
#ifndef IS_SALAMANDER
   KPADInit();
   WPADEnableURCC(true);
   WPADEnableWiiRemote(true);
#endif
}

static void deinit_pad_libraries(void)
{
#ifndef IS_SALAMANDER
   KPADShutdown();
#endif
}

static void do_logging_init(void)
{
#if defined(PC_DEVELOPMENT_IP_ADDRESS) && defined(PC_DEVELOPMENT_TCP_PORT)
   wiiu_log_init(PC_DEVELOPMENT_IP_ADDRESS, PC_DEVELOPMENT_TCP_PORT);
   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
#endif
}

static void do_logging_deinit(void)
{
   fflush(stdout);
   fflush(stderr);

#if defined(PC_DEVELOPMENT_IP_ADDRESS) && defined(PC_DEVELOPMENT_TCP_PORT)
   wiiu_log_deinit();
#endif
}

static void do_rarch_main(int argc, char **argv)
{
#if 0
   int argc_ = 2;
//   char* argv_[] = {WIIU_SD_PATH "retroarch/retroarch.elf", WIIU_SD_PATH "rom.nes", NULL};
   char *argv_[] = {WIIU_SD_PATH "retroarch/retroarch.elf", WIIU_SD_PATH "rom.sfc", NULL};

   rarch_main(argc_, argv_, NULL);
#else /* #if 0 */
   rarch_main(argc, argv, NULL);
#endif /* #if 0 */
}

static void main_loop(void)
{
   unsigned sleep_ms = 0;
   OSTime start_time;
   int status;

   do
   {
      if(video_driver_get_ptr(false))
      {
         start_time = OSGetSystemTime();
         task_queue_wait(swap_is_pending, &start_time);
      }
      else
         task_queue_wait(NULL, NULL);

      status = runloop_iterate(&sleep_ms);

      if (status == 1 && sleep_ms > 0)
         retro_sleep(sleep_ms);

      if (status == -1)
         break;
   }
   while (1);
}

static void main_init(void)
{
   setup_os_exceptions();
   ProcUIInit(&SaveCallback);
   do_network_init();
   do_logging_init();
   init_pad_libraries();
   verbosity_enable();
   fflush(stdout);
}

static void main_deinit(void)
{
   deinit_pad_libraries();
   ProcUIShutdown();

   do_logging_deinit();
}

static void read_argc_argv(int *argc, char ***argv)
{
   DEBUG_VAR(ARGV_PTR);
   if(ARGV_PTR && ((u32)ARGV_PTR < 0x01000000))
   {
      struct
      {
         u32 magic;
         u32 argc;
         char * argv[3];
      }*param = ARGV_PTR;
      if(param->magic == ARGV_MAGIC)
      {
         *argc = param->argc;
         *argv = param->argv;
      }
      ARGV_PTR = NULL;
   }

   DEBUG_VAR(argc);
   DEBUG_STR(argv[0]);
   DEBUG_STR(argv[1]);
   fflush(stdout);
}

int main(int argc, char **argv)
{
   main_init();
   read_argc_argv(&argc, &argv);

#ifdef IS_SALAMANDER
   int salamander_main(int, char **);
   salamander_main(argc, argv);
#else
   do_rarch_main(argc, argv);
   main_loop();
   main_exit(NULL);
#endif /* IS_SALAMANDER */
   main_deinit();

   /* returning non 0 here can prevent loading a different rpx/elf in the HBL
      environment */
   return 0;
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

static int mcp_hook_fd = -1;

int MCPHookOpen(void)
{
   //take over mcp thread
   mcp_hook_fd = IOS_Open("/dev/mcp", 0);

   if (mcp_hook_fd < 0)
      return -1;

   IOS_IoctlAsync(mcp_hook_fd, 0x62, (void *)0, 0, (void *)0, 0, someFunc, (void *)0);
   //let wupserver start up
   retro_sleep(1000);

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
   retro_sleep(1000);
   IOS_Close(mcp_hook_fd);
   mcp_hook_fd = -1;
}


static int iosuhaxMount = 0;

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


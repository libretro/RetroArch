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

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <boolean.h>

#include <file/file_path.h>

#include <lists/file_list.h>

#include "../frontend_driver.h"
#include "../frontend.h"
#include "../../verbosity.h"
#include "../../defaults.h"
#include "../../paths.h"
#include "retroarch.h"
#include "file_path_special.h"
#include "audio/audio_driver.h"


#include "tasks/tasks_internal.h"
#include "runloop.h"
#include <sys/socket.h>
#include "fs/fs_utils.h"
#include "fs/sd_fat_devoptab.h"
#include "system/dynamic.h"
#include "system/memory.h"
#include "system/exception_handler.h"
#include "system/exception.h"
#include <sys/iosupport.h>

#include <wiiu/os/foreground.h>
#include <wiiu/procui.h>
#include <wiiu/sysapp.h>
#include <wiiu/ios.h>
#include <wiiu/vpad.h>
#include <wiiu/kpad.h>

#include <fat.h>
#include <iosuhax.h>

#include "wiiu_dbg.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

//#define WIIU_SD_PATH "/vol/external01/"
#define WIIU_SD_PATH "sd:/"
#define WIIU_USB_PATH "usb:/"

static enum frontend_fork wiiu_fork_mode = FRONTEND_FORK_NONE;
static const char* elf_path_cst = WIIU_SD_PATH "retroarch/retroarch.elf";

static void frontend_wiiu_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   (void)args;
   DEBUG_LINE();

   fill_pathname_basedir(g_defaults.dir.port, elf_path_cst, sizeof(g_defaults.dir.port));
   DEBUG_LINE();
   RARCH_LOG("port dir: [%s]\n", g_defaults.dir.port);

   fill_pathname_join(g_defaults.dir.core_assets, g_defaults.dir.port,
         "downloads", sizeof(g_defaults.dir.core_assets));
   fill_pathname_join(g_defaults.dir.assets, g_defaults.dir.port,
         "media", sizeof(g_defaults.dir.assets));
   fill_pathname_join(g_defaults.dir.core, g_defaults.dir.port,
         "cores", sizeof(g_defaults.dir.core));
   fill_pathname_join(g_defaults.dir.core_info, g_defaults.dir.core,
         "info", sizeof(g_defaults.dir.core_info));
   fill_pathname_join(g_defaults.dir.savestate, g_defaults.dir.core,
         "savestates", sizeof(g_defaults.dir.savestate));
   fill_pathname_join(g_defaults.dir.sram, g_defaults.dir.core,
         "savefiles", sizeof(g_defaults.dir.sram));
   fill_pathname_join(g_defaults.dir.system, g_defaults.dir.core,
         "system", sizeof(g_defaults.dir.system));
   fill_pathname_join(g_defaults.dir.playlist, g_defaults.dir.core,
         "playlists", sizeof(g_defaults.dir.playlist));
   fill_pathname_join(g_defaults.dir.menu_config, g_defaults.dir.port,
         "config", sizeof(g_defaults.dir.menu_config));
   fill_pathname_join(g_defaults.dir.remap, g_defaults.dir.port,
         "config/remaps", sizeof(g_defaults.dir.remap));
   fill_pathname_join(g_defaults.dir.video_filter, g_defaults.dir.port,
         "filters", sizeof(g_defaults.dir.remap));
   fill_pathname_join(g_defaults.dir.database, g_defaults.dir.port,
         "database/rdb", sizeof(g_defaults.dir.database));
   fill_pathname_join(g_defaults.dir.cursor, g_defaults.dir.port,
         "database/cursors", sizeof(g_defaults.dir.cursor));
   fill_pathname_join(g_defaults.path.config, g_defaults.dir.port,
         file_path_str(FILE_PATH_MAIN_CONFIG), sizeof(g_defaults.path.config));
}

static void frontend_wiiu_deinit(void *data)
{
   (void)data;
}

static void frontend_wiiu_shutdown(bool unused)
{
   (void)unused;
}

static void frontend_wiiu_init(void *data)
{
   (void)data;
   DEBUG_LINE();
   verbosity_enable();
   DEBUG_LINE();
}


static int frontend_wiiu_get_rating(void)
{
   return 10;
}

enum frontend_architecture frontend_wiiu_get_architecture(void)
{
   return FRONTEND_ARCH_PPC;
}

static int frontend_wiiu_parse_drive_list(void *data)
{
   file_list_t *list = (file_list_t*)data;

   if (!list)
      return -1;

   menu_entries_append_enum(list, WIIU_SD_PATH,
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
         MENU_SETTING_ACTION, 0, 0);

   menu_entries_append_enum(list, WIIU_USB_PATH,
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,
         MENU_SETTING_ACTION, 0, 0);

   return 0;
}

frontend_ctx_driver_t frontend_ctx_wiiu = {
   frontend_wiiu_get_environment_settings,
   frontend_wiiu_init,
   frontend_wiiu_deinit,
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   frontend_wiiu_shutdown,
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_wiiu_get_rating,
   NULL,                         /* load_content */
   frontend_wiiu_get_architecture,
   NULL,                         /* get_powerstate */
   frontend_wiiu_parse_drive_list,
   NULL,                         /* get_mem_total */
   NULL,                         /* get_mem_free */
   NULL,                         /* install_signal_handler */
   NULL,                         /* get_signal_handler_state */
   NULL,                         /* set_signal_handler_state */
   NULL,                         /* destroy_signal_handler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   "wiiu",
};

static int log_socket = -1;
static volatile int log_lock = 0;

void log_init(const char * ipString, int port)
{
	log_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (log_socket < 0)
		return;

	struct sockaddr_in connect_addr;
	memset(&connect_addr, 0, sizeof(connect_addr));
	connect_addr.sin_family = AF_INET;
	connect_addr.sin_port = port;
	inet_aton(ipString, &connect_addr.sin_addr);

	if(connect(log_socket, (struct sockaddr*)&connect_addr, sizeof(connect_addr)) < 0)
	{
	    socketclose(log_socket);
	    log_socket = -1;
	}
}

void log_deinit(void)
{
    if(log_socket >= 0)
    {
        socketclose(log_socket);
        log_socket = -1;
    }
}
static ssize_t log_write(struct _reent *r, void* fd, const char *ptr, size_t len)
{
   if(log_socket < 0)
       return len;

   while(log_lock)
      OSSleepTicks(((248625000/4)) / 1000);
   log_lock = 1;

   int ret;
   while (len > 0) {
       int block = len < 1400 ? len : 1400; // take max 1400 bytes per UDP packet
       ret = send(log_socket, ptr, block, 0);
       if(ret < 0)
           break;

       len -= ret;
       ptr += ret;
   }

   log_lock = 0;

   return len;
}
void net_print(const char* str)
{
   log_write(NULL, 0, str, strlen(str));
}

void net_print_exp(const char* str)
{
   send(log_socket, str, strlen(str), 0);
}

static devoptab_t dotab_stdout = {
   "stdout_net", // device name
   0,            // size of file structure
   NULL,         // device open
   NULL,         // device close
   log_write,    // device write
   NULL,
   /* ... */
};

void SaveCallback()
{
   OSSavesDone_ReadyToRelease();
}

int main(int argc, char **argv)
{   
#if 1
   setup_os_exceptions();
#else
   InstallExceptionHandler();
#endif

   ProcUIInit(&SaveCallback);

   socket_lib_init();
#if defined(PC_DEVELOPMENT_IP_ADDRESS) && defined(PC_DEVELOPMENT_TCP_PORT)
   log_init(PC_DEVELOPMENT_IP_ADDRESS, PC_DEVELOPMENT_TCP_PORT);
   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
#endif
   VPADInit();
   WPADEnableURCC(true);
   WPADEnableWiiRemote(true);
   KPADInit();

   verbosity_enable();
   DEBUG_VAR(argc);
   DEBUG_STR(argv[0]);
   DEBUG_STR(argv[1]);
   fflush(stdout);

#if 1
#if 0
   int argc_ = 2;
//   char* argv_[] = {WIIU_SD_PATH "retroarch/retroarch.elf", WIIU_SD_PATH "rom.nes", NULL};
   char* argv_[] = {WIIU_SD_PATH "retroarch/retroarch.elf", WIIU_SD_PATH "rom.sfc", NULL};

   rarch_main(argc_, argv_, NULL);
#else
   rarch_main(argc, argv, NULL);
#endif
   do
   {
      unsigned sleep_ms = 0;
      int ret = runloop_iterate(&sleep_ms);

      if (ret == 1 && sleep_ms > 0)
       retro_sleep(sleep_ms);
      task_queue_ctl(TASK_QUEUE_CTL_WAIT, NULL);
      if (ret == -1)
       break;

   }while(1);

   main_exit(NULL);
#endif
   fflush(stdout);
   fflush(stderr);
   ProcUIShutdown();

#if defined(PC_DEVELOPMENT_IP_ADDRESS) && defined(PC_DEVELOPMENT_TCP_PORT)
   log_deinit();
#endif
   return 0;
}

unsigned long _times_r(struct _reent *r, struct tms *tmsbuf)
{
   return 0;
}

void __eabi()
{

}

__attribute__((weak))
void __init(void)
{
   extern void(*__CTOR_LIST__[])(void);
   void(**ctor)(void) = __CTOR_LIST__;
   while(*ctor)
      (*ctor++)();
}


__attribute__((weak))
void __fini(void)
{
   extern void(*__DTOR_LIST__[])(void);
   void(**ctor)(void) = __DTOR_LIST__;
   while(*ctor)
      (*ctor++)();
}

/* libiosuhax related */

//just to be able to call async
void someFunc(void *arg)
{
    (void)arg;
}

static int mcp_hook_fd = -1;
int MCPHookOpen()
{
    //take over mcp thread
    mcp_hook_fd = IOS_Open("/dev/mcp", 0);
    if(mcp_hook_fd < 0)
        return -1;
    IOS_IoctlAsync(mcp_hook_fd, 0x62, (void*)0, 0, (void*)0, 0, someFunc, (void*)0);
    //let wupserver start up
    retro_sleep(1000);
    if(IOSUHAX_Open("/dev/mcp") < 0)
    {
        IOS_Close(mcp_hook_fd);
        mcp_hook_fd = -1;
        return -1;
    }
    return 0;
}

void MCPHookClose()
{
    if(mcp_hook_fd < 0)
        return;
    //close down wupserver, return control to mcp
    IOSUHAX_Close();
    //wait for mcp to return
    retro_sleep(1000);
    IOS_Close(mcp_hook_fd);
    mcp_hook_fd = -1;
}

/* HBL elf entry point */
int __entry_menu(int argc, char **argv)
{
   InitFunctionPointers();
   memoryInitialize();

   int iosuhaxMount = 0;
   int res = IOSUHAX_Open(NULL);
   if(res < 0)
      res = MCPHookOpen();

   if(res < 0)
      mount_sd_fat("sd");
   else
   {
      iosuhaxMount = 1;
      fatInitDefault();
   }

   __init();
   int ret = main(argc, argv);
   __fini();

   if(iosuhaxMount)
   {
      fatUnmount("sd:");
      fatUnmount("usb:");
      if(mcp_hook_fd >= 0)
         MCPHookClose();
      else
         IOSUHAX_Close();
   }
   else
      unmount_sd_fat("sd");

   memoryRelease();
   return ret;
}

/* RPX entry point */
__attribute__((noreturn))
void _start(int argc, char **argv)
{
   memoryInitialize();

   int iosuhaxMount = 0;
   int res = IOSUHAX_Open(NULL);
   if(res < 0)
      res = MCPHookOpen();

   if(res < 0)
      mount_sd_fat("sd");
   else
   {
      iosuhaxMount = 1;
      fatInitDefault();
   }

   __init();
   int ret = main(argc, argv);
   __fini();

   if(iosuhaxMount)
   {
      fatUnmount("sd:");
      fatUnmount("usb:");
      if(mcp_hook_fd >= 0)
         MCPHookClose();
      else
         IOSUHAX_Close();
   }
   else
      unmount_sd_fat("sd");

   memoryRelease();
   SYSRelaunchTitle(argc, argv);
   exit(ret);
}

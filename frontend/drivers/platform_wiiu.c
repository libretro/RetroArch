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
#include <string/stdstring.h>
#include <retro_timers.h>

#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#include "../frontend_driver.h"
#include "../frontend.h"
#include "../../verbosity.h"
#include "../../defaults.h"
#include "../../paths.h"
#include "retroarch.h"
#include "file_path_special.h"
#include "audio/audio_driver.h"


#include "tasks/tasks_internal.h"
#include "../../retroarch.h"
#include <net/net_compat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "fs/fs_utils.h"
#include "fs/sd_fat_devoptab.h"
#include "system/dynamic.h"
#include "system/memory.h"
#include "system/exception_handler.h"
#include <sys/iosupport.h>

#include <wiiu/os/foreground.h>
#include <wiiu/gx2/event.h>
#include <wiiu/procui.h>
#include <wiiu/sysapp.h>
#include <wiiu/ios.h>
#include <wiiu/vpad.h>
#include <wiiu/kpad.h>

#include "wiiu/controller_patcher/ControllerPatcherWrapper.h"

#include <fat.h>
#include <iosuhax.h>
#include "wiiu_dbg.h"
#include "hbl.h"

#ifndef IS_SALAMANDER
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#endif

//#define WIIU_SD_PATH "/vol/external01/"
#define WIIU_SD_PATH "sd:/"
#define WIIU_USB_PATH "usb:/"

static enum frontend_fork wiiu_fork_mode = FRONTEND_FORK_NONE;
static const char *elf_path_cst = WIIU_SD_PATH "retroarch/retroarch.elf";

static void frontend_wiiu_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   unsigned i;
   (void)args;

   fill_pathname_basedir(g_defaults.dirs[DEFAULT_DIR_PORT], elf_path_cst, sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], g_defaults.dirs[DEFAULT_DIR_PORT],
         "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], g_defaults.dirs[DEFAULT_DIR_PORT],
         "media", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], g_defaults.dirs[DEFAULT_DIR_PORT],
         "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], g_defaults.dirs[DEFAULT_DIR_CORE],
         "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], g_defaults.dirs[DEFAULT_DIR_CORE],
         "savestates", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], g_defaults.dirs[DEFAULT_DIR_CORE],
         "savefiles", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], g_defaults.dirs[DEFAULT_DIR_CORE],
         "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], g_defaults.dirs[DEFAULT_DIR_CORE],
         "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], g_defaults.dirs[DEFAULT_DIR_PORT],
         "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], g_defaults.dirs[DEFAULT_DIR_PORT],
         "config/remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], g_defaults.dirs[DEFAULT_DIR_PORT],
         "filters", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], g_defaults.dirs[DEFAULT_DIR_PORT],
         "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], g_defaults.dirs[DEFAULT_DIR_PORT],
         "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
   fill_pathname_join(g_defaults.path.config, g_defaults.dirs[DEFAULT_DIR_PORT],
         file_path_str(FILE_PATH_MAIN_CONFIG), sizeof(g_defaults.path.config));

   for (i = 0; i < DEFAULT_DIR_LAST; i++)
   {
      const char *dir_path = g_defaults.dirs[i];
      if (!string_is_empty(dir_path))
         path_mkdir(dir_path);
   }
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

static int frontend_wiiu_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t *)data;
   enum msg_hash_enums enum_idx = load_content ?
      MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
      MSG_UNKNOWN;

   if (!list)
      return -1;

   menu_entries_append_enum(list, WIIU_SD_PATH,
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);

   menu_entries_append_enum(list, WIIU_USB_PATH,
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#endif
   return 0;
}


static void frontend_wiiu_exec(const char *path, bool should_load_game)
{

   struct
   {
      u32 magic;
      u32 argc;
      char * argv[3];
      char args[];
   }*param = getApplicationEndAddr();
   int len = 0;
   param->argc = 0;

   if(!path || !*path)
   {
      RARCH_LOG("No executable path provided, cannot Restart\n");
   }

   DEBUG_STR(path);

   strcpy(param->args + len, elf_path_cst);
   param->argv[param->argc] = param->args + len;
   len += strlen(param->args + len) + 1;
   param->argc++;

   RARCH_LOG("Attempt to load core: [%s].\n", path);
#ifndef IS_SALAMANDER
   if (should_load_game && !path_is_empty(RARCH_PATH_CONTENT))
   {
      strcpy(param->args + len, path_get(RARCH_PATH_CONTENT));
      param->argv[param->argc] = param->args + len;
      len += strlen(param->args + len) + 1;
      param->argc++;

      RARCH_LOG("content path: [%s].\n", path_get(RARCH_PATH_CONTENT));
   }
#endif
   param->argv[param->argc] = NULL;

   {
      if (HBL_loadToMemory(path, (u32)param->args - (u32)param + len) < 0)
         RARCH_LOG("Failed to load core\n");
      else
      {
         param->magic = ARGV_MAGIC;
         ARGV_PTR = param;
         DEBUG_VAR(param->argc);
         DEBUG_VAR(param->argv);

      }
   }
}

#ifndef IS_SALAMANDER
static bool frontend_wiiu_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         RARCH_LOG("FRONTEND_FORK_CORE\n");
         wiiu_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         RARCH_LOG("FRONTEND_FORK_CORE_WITH_ARGS\n");
         wiiu_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         RARCH_LOG("FRONTEND_FORK_RESTART\n");
         /* NOTE: We don't implement Salamander, so just turn
          * this into FRONTEND_FORK_CORE. */
         wiiu_fork_mode  = FRONTEND_FORK_CORE;
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}
#endif

static void frontend_wiiu_exitspawn(char *s, size_t len)
{
   bool should_load_game = false;
#ifndef IS_SALAMANDER
   if (wiiu_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (wiiu_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_game = true;
         break;
      default:
         break;
   }
#endif
   frontend_wiiu_exec(s, should_load_game);
}


frontend_ctx_driver_t frontend_ctx_wiiu =
{
   frontend_wiiu_get_environment_settings,
   frontend_wiiu_init,
   frontend_wiiu_deinit,
   frontend_wiiu_exitspawn,
   NULL,                         /* process_args */
   frontend_wiiu_exec,
#ifdef IS_SALAMANDER
   NULL,                         /* set_fork */
#else
   frontend_wiiu_set_fork,
#endif
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

static int wiiu_log_socket = -1;
static volatile int wiiu_log_lock = 0;

void wiiu_log_init(const char *ipString, int port)
{
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

   while (len > 0)
   {
      int block = len < 1400 ? len : 1400; // take max 1400 bytes per UDP packet
      ret = send(wiiu_log_socket, ptr, block, 0);

      if (ret < 0)
         break;

      len -= ret;
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

void SaveCallback()
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

int main(int argc, char **argv)
{
   setup_os_exceptions();
   ProcUIInit(&SaveCallback);

#ifdef IS_SALAMANDER
   socket_lib_init();
#else
   network_init();
#endif
#if defined(PC_DEVELOPMENT_IP_ADDRESS) && defined(PC_DEVELOPMENT_TCP_PORT)
   wiiu_log_init(PC_DEVELOPMENT_IP_ADDRESS, PC_DEVELOPMENT_TCP_PORT);
   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
#endif
#ifndef IS_SALAMANDER
   VPADInit();
   WPADEnableURCC(true);
   WPADEnableWiiRemote(true);
   KPADInit();
#endif
   verbosity_enable();
#ifndef IS_SALAMANDER
   ControllerPatcherInit();
#endif
   fflush(stdout);
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
         argc = param->argc;
         argv = param->argv;
      }
      ARGV_PTR = NULL;
   }

   DEBUG_VAR(argc);
   DEBUG_STR(argv[0]);
   DEBUG_STR(argv[1]);
   fflush(stdout);
#ifdef IS_SALAMANDER
   int salamander_main(int, char **);
   salamander_main(argc, argv);
#else
#if 1
#if 0
   int argc_ = 2;
//   char* argv_[] = {WIIU_SD_PATH "retroarch/retroarch.elf", WIIU_SD_PATH "rom.nes", NULL};
   char *argv_[] = {WIIU_SD_PATH "retroarch/retroarch.elf", WIIU_SD_PATH "rom.sfc", NULL};

   rarch_main(argc_, argv_, NULL);
#else
   rarch_main(argc, argv, NULL);
#endif
   do
   {
      unsigned sleep_ms = 0;

      if(video_driver_get_ptr(false))
      {
         OSTime start_time = OSGetSystemTime();
         task_queue_wait(swap_is_pending, &start_time);
      }
      else
         task_queue_wait(NULL, NULL);

      int ret = runloop_iterate(&sleep_ms);

      if (ret == 1 && sleep_ms > 0)
         retro_sleep(sleep_ms);


      if (ret == -1)
         break;

   }
   while (1);
#ifndef IS_SALAMANDER
   ControllerPatcherDeInit();
#endif
   main_exit(NULL);
#endif
#endif
   fflush(stdout);
   fflush(stderr);
   ProcUIShutdown();

#if defined(PC_DEVELOPMENT_IP_ADDRESS) && defined(PC_DEVELOPMENT_TCP_PORT)
   wiiu_log_deinit();
#endif

   /* returning non 0 here can prevent loading a different rpx/elf in the HBL environment */
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

   while (*ctor)
      (*ctor++)();
}


__attribute__((weak))
void __fini(void)
{
   extern void(*__DTOR_LIST__[])(void);
   void(**ctor)(void) = __DTOR_LIST__;

   while (*ctor)
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

void MCPHookClose()
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

/* HBL elf entry point */
int __entry_menu(int argc, char **argv)
{
   InitFunctionPointers();
   memoryInitialize();
   __init();
   fsdev_init();

   int ret = main(argc, argv);

   fsdev_exit();
//   __fini();
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

   int ret = main(argc, argv);

   fsdev_exit();
//   __fini();
   memoryRelease();
   SYSRelaunchTitle(0, 0);
   exit(0);
}

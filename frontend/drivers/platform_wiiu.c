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
#include <unistd.h>
#include <sys/iosupport.h>
#include <net/net_compat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <wiiu/types.h>
#include <wiiu/ac.h>
#include <file/file_path.h>

#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#include <string/stdstring.h>

#include <wiiu/gx2.h>
#include <wiiu/kpad.h>
#include <wiiu/ios.h>
#include <wiiu/os.h>
#include <wiiu/procui.h>
#include <wiiu/sysapp.h>

#include "file_path_special.h"

#include "../frontend.h"
#include "../frontend_driver.h"
#include "../../defaults.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#include "hbl.h"
#include "wiiu_dbg.h"
#include "system/exception_handler.h"
#include "tasks/tasks_internal.h"

#ifndef IS_SALAMANDER
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#endif

#define WIIU_SD_PATH "sd:/"
#define WIIU_USB_PATH "usb:/"

/**
 * The Wii U frontend driver, along with the main() method.
 */

static enum frontend_fork wiiu_fork_mode = FRONTEND_FORK_NONE;
static const char *elf_path_cst = WIIU_SD_PATH "retroarch/retroarch.elf";

static bool exists(char *path) {
   struct stat stat_buf = {0};

   if(!path)
      return false;

   return (stat(path, &stat_buf) == 0);
}

static void fix_asset_directory(void) {
   char src_path_buf[PATH_MAX_LENGTH] = {0};
   char dst_path_buf[PATH_MAX_LENGTH] = {0};

   fill_pathname_join(src_path_buf, g_defaults.dirs[DEFAULT_DIR_PORT], "media", sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   fill_pathname_join(dst_path_buf, g_defaults.dirs[DEFAULT_DIR_PORT], "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

   if(exists(dst_path_buf) || !exists(src_path_buf))
      return;

   rename(src_path_buf, dst_path_buf);
}

static void frontend_wiiu_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   unsigned i;
   (void)args;

   fill_pathname_basedir(g_defaults.dirs[DEFAULT_DIR_PORT], elf_path_cst, sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], g_defaults.dirs[DEFAULT_DIR_PORT],
         "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fix_asset_directory();
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], g_defaults.dirs[DEFAULT_DIR_PORT],
         "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
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
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], g_defaults.dirs[DEFAULT_DIR_CORE],
         "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
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
      MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;

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
   }*param     = getApplicationEndAddr();
   int len     = 0;
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
   NULL,                         /* watch_path_for_changes */
   NULL,                         /* check_for_path_changes */
   NULL,                         /* set_sustained_performance_mode */
   NULL,                         /* get_cpu_model_name */
   NULL,                         /* get_user_language */
   "wiiu",
   NULL,                         /* get_video_driver */
};

/* main() and its supporting functions */

static void main_setup(void);
static void get_arguments(int *argc, char ***argv);
static void main_loop(void);
static void main_teardown(void);

static void init_network(void);
static void deinit_network(void);
static void init_logging(void);
static void deinit_logging(void);
static void wiiu_log_init(int port);
static void wiiu_log_deinit(void);
static ssize_t wiiu_log_write(struct _reent *r, void *fd, const char *ptr, size_t len);
static void init_pad_libraries(void);
static void deinit_pad_libraries(void);
static void SaveCallback(void);
static bool swap_is_pending(void *start_time);

static struct sockaddr_in broadcast;
static int wiiu_log_socket = -1;
static volatile int wiiu_log_lock = 0;

#if !defined(PC_DEVELOPMENT_TCP_PORT)
#define PC_DEVELOPMENT_TCP_PORT 4405
#endif

static devoptab_t dotab_stdout =
{
   "stdout_net",   /* device name */
   0,              /* size of file structure */
   NULL,           /* device open */
   NULL,           /* device close */
   wiiu_log_write, /* device write */
   NULL,           /* ... */
};

int main(int argc, char **argv)
{
   main_setup();
   get_arguments(&argc, &argv);

#ifdef IS_SALAMANDER
   int salamander_main(int argc, char **argv);
   salamander_main(argc, argv);
#else
   rarch_main(argc, argv, NULL);
   main_loop();
   main_exit(NULL);
#endif /* IS_SALAMANDER */
   main_teardown();

   /* We always return 0 because if we don't, it can prevent loading a
    * different RPX/ELF in HBL. */
   return 0;
}

static void get_arguments(int *argc, char ***argv)
{
   DEBUG_VAR(ARGV_PTR);
   if(ARGV_PTR && ((u32)ARGV_PTR < 0x01000000))
   {
      struct
      {
         u32 magic;
         u32 argc;
         char *argv[3];
      } *param = ARGV_PTR;
      if(param->magic == ARGV_MAGIC)
      {
        *argc = param->argc;
        *argv = param->argv;
      }
      ARGV_PTR = NULL;
   }

   DEBUG_VAR(argc);
   DEBUG_VAR(argv[0]);
   DEBUG_VAR(argv[1]);
   fflush(stdout);
}

static void main_setup(void)
{
   setup_os_exceptions();
   ProcUIInit(&SaveCallback);
   init_network();
   init_logging();
   init_pad_libraries();
   verbosity_enable();
   fflush(stdout);
}

static void main_teardown(void)
{
   deinit_pad_libraries();
   ProcUIShutdown();
   deinit_logging();
   deinit_network();
}

static void main_loop(void)
{
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

      status = runloop_iterate();

      if (status == -1)
         break;
   } while(true);
}

static void SaveCallback(void)
{
   OSSavesDone_ReadyToRelease();
}

static bool swap_is_pending(void *start_time)
{
   uint32_t swap_count, flip_count;
   OSTime last_flip, last_vsync;

   GX2GetSwapStatus(&swap_count, &flip_count, &last_flip, &last_vsync);
   return last_vsync < *(OSTime *)start_time;
}

static void init_network(void)
{
   ACInitialize();
   ACConnect();
#ifdef IS_SALAMANDER
   socket_lib_init();
#else
   network_init();
#endif /* IS_SALAMANDER */
}

static void deinit_network(void)
{
   ACClose();
   ACFinalize();
}

int getBroadcastAddress(ACIpAddress *broadcast)
{
   ACIpAddress myIp, mySubnet;
   ACResult result;

   if(broadcast == NULL)
      return -1;

   result = ACGetAssignedAddress(&myIp);
   if(result < 0)
      return -1;
   result = ACGetAssignedSubnet(&mySubnet);
   if(result < 0)
      return -1;

   *broadcast = myIp | (~mySubnet);
   return 0;
}

static void init_logging(void)
{
   wiiu_log_init(PC_DEVELOPMENT_TCP_PORT);
   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
}

static void deinit_logging(void)
{
   fflush(stdout);
   fflush(stderr);

   wiiu_log_deinit();
}

static int broadcast_init(int port)
{
   ACIpAddress broadcast_ip;
   if(getBroadcastAddress(&broadcast_ip) < 0)
      return -1;

   memset(&broadcast, 0, sizeof(broadcast));
   broadcast.sin_family = AF_INET;
   broadcast.sin_port = htons(port);
   broadcast.sin_addr.s_addr = htonl(broadcast_ip);

   return 0;
}

static void wiiu_log_init(int port)
{
   wiiu_log_lock = 0;

   if(wiiu_log_socket >= 0)
      return;

   if(broadcast_init(port) < 0)
      return;

   wiiu_log_socket = socket(AF_INET, SOCK_DGRAM, 0);

   if(wiiu_log_socket < 0)
      return;

   struct sockaddr_in connect_addr;
   memset(&connect_addr, 0, sizeof(connect_addr));
   connect_addr.sin_family = AF_INET;
   connect_addr.sin_port = 0;
   connect_addr.sin_addr.s_addr = htonl(INADDR_ANY);

   if( bind(wiiu_log_socket, (struct sockaddr *)&connect_addr, sizeof(connect_addr)) < 0)
   {
      socketclose(wiiu_log_socket);
      wiiu_log_socket = -1;
      return;
   }
}

static void wiiu_log_deinit(void)
{
   if(wiiu_log_socket >= 0)
   {
      socketclose(wiiu_log_socket);
      wiiu_log_socket = -1;
   }
}

static void init_pad_libraries(void)
{
#ifndef IS_SALAMANDER
   KPADInit();
   WPADEnableURCC(true);
   WPADEnableWiiRemote(true);
#endif /* IS_SALAMANDER */
}

static void deinit_pad_libraries(void)
{
#ifndef IS_SALAMANDER
   KPADShutdown();
#endif /* IS_SALAMANDER */
}

/* logging routines */

void net_print(const char *str)
{
   wiiu_log_write(NULL, 0, str, strlen(str));
}

void net_print_exp(const char *str)
{
   sendto(wiiu_log_socket, str, strlen(str), 0, (struct sockaddr *)&broadcast, sizeof(broadcast));
}

/* RFC 791 specifies that any IP host must be able to receive a datagram of 576 bytes.
 * Since we're generally never logging more than a line or two's worth of data (~100 bytes)
 * this is a reasonable size for our use. */
#define DGRAM_SIZE 576

static ssize_t wiiu_log_write(struct _reent *r, void *fd, const char *ptr, size_t len)
{
   if( wiiu_log_socket < 0)
      return len;

   while(wiiu_log_lock)
      OSSleepTicks(((248625000 / 4)) / 1000);

   wiiu_log_lock = 1;

   int sent;
   int remaining = len;

   while(remaining > 0)
   {
      int block = remaining < DGRAM_SIZE ? remaining : DGRAM_SIZE;
      sent = sendto(wiiu_log_socket, ptr, block, 0, (struct sockaddr *)&broadcast, sizeof(broadcast));

      if(sent < 0)
         break;

      remaining -= sent;
      ptr       += sent;
   }

   wiiu_log_lock = 0;

   return len;
}

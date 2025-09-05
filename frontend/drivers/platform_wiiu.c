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

#include <wiiu/types.h>
#include <file/file_path.h>

#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#include <string/stdstring.h>

#include "../frontend.h"
#include "../frontend_driver.h"
#include "../../file_path_special.h"
#include "../../defaults.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#ifndef IS_SALAMANDER
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#endif
#endif

#if defined(HAVE_LIBMOCHA) && defined(HAVE_LIBFAT)
#include <fat.h>
#include <mocha/disc_interface.h>
#include <mocha/mocha.h>
#endif

#include "system/memory.h"
#include "system/exception_handler.h"

#include "wiiu_dbg.h"

#include <coreinit/dynload.h>
#include <proc_ui/procui.h>
#include <gx2/event.h>
#include <coreinit/foreground.h>
#include <padscore/kpad.h>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_module.h>
#include <whb/log_udp.h>

#define WIIU_SD_PATH "sd:/"
#define WIIU_USB_PATH "usb:/"
#define WIIU_STORAGE_USB_PATH "storage_usb:/"

/**
 * The Wii U frontend driver, along with the main() method.
 */

#ifndef IS_SALAMANDER
static bool have_libfat_usb = false;
static bool have_libfat_sdcard = false;
static bool have_wfs_usb = false;
#endif
static bool in_aroma = false;

static bool exists(char* path)
{
   struct stat stat_buf = {0};

   if (!path)
      return false;

   return (stat(path, &stat_buf) == 0);
}

static void fix_asset_directory(void)
{
   char src_path_buf[PATH_MAX_LENGTH] = {0};
   char dst_path_buf[PATH_MAX_LENGTH] = {0};

   fill_pathname_join(src_path_buf, g_defaults.dirs[DEFAULT_DIR_PORT], "media",
                      sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   fill_pathname_join(dst_path_buf, g_defaults.dirs[DEFAULT_DIR_PORT], "assets",
                      sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

   if (exists(dst_path_buf) || !exists(src_path_buf))
      return;

   rename(src_path_buf, dst_path_buf);
}

static void frontend_wiiu_get_env_settings(int*  argc, char* argv[],
                                           void* args, void* params_data)
{
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PORT], WIIU_SD_PATH,
                      "retroarch", sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

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
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], g_defaults.dirs[DEFAULT_DIR_CORE],
                      "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "screenshots", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.path_config, g_defaults.dirs[DEFAULT_DIR_PORT],
                      FILE_PATH_MAIN_CONFIG, sizeof(g_defaults.path_config));

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
#endif
}

static void frontend_wiiu_deinit(void* data)
{
   (void)data;
}

static void frontend_wiiu_shutdown(bool unused)
{
   (void)unused;
}

static void frontend_wiiu_init(void* data)
{
   (void)data;
   DEBUG_LINE();
   verbosity_enable();
   DEBUG_LINE();
}

static int frontend_wiiu_get_rating(void) { return 10; }

enum frontend_architecture frontend_wiiu_get_arch(void)
{
   return FRONTEND_ARCH_PPC;
}

static int frontend_wiiu_parse_drive_list(void* data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t*        list     = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content
                                     ? MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR
                                     : MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;

   if (!list)
      return -1;

   menu_entries_append(list, WIIU_SD_PATH,
                       msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                       enum_idx,
                       FILE_TYPE_DIRECTORY, 0, 0, NULL);
   if (have_libfat_usb)
      menu_entries_append(list, WIIU_USB_PATH,
                          msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                          enum_idx,
                          FILE_TYPE_DIRECTORY, 0, 0, NULL);
   if (have_wfs_usb)
      menu_entries_append(list, WIIU_STORAGE_USB_PATH,
                          msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
                          enum_idx,
                          FILE_TYPE_DIRECTORY, 0, 0, NULL);
#endif
   return 0;
}

frontend_ctx_driver_t frontend_ctx_wiiu =
{
   frontend_wiiu_get_env_settings,
   frontend_wiiu_init,
   frontend_wiiu_deinit,
   NULL, /* exitspawn */
   NULL, /* process_args */
   NULL, /* exec */
#ifdef IS_SALAMANDER
   NULL, /* set_fork */
#else
   NULL, /* set_fork */
#endif
   frontend_wiiu_shutdown,
   NULL, /* get_name */
   NULL, /* get_os */
   frontend_wiiu_get_rating,
   NULL,                   /* content_loaded */
   frontend_wiiu_get_arch, /* get_architecture */
   NULL,                   /* get_powerstate */
   frontend_wiiu_parse_drive_list,
   NULL,   /* get_total_mem */
   NULL,   /* get_free_mem */
   NULL,   /* install_signal_handler */
   NULL,   /* get_signal_handler_state */
   NULL,   /* set_signal_handler_state       */
   NULL,   /* destroy_signal_handler_state   */
   NULL,   /* attach_console                 */
   NULL,   /* detach_console                 */
   NULL,   /* get_lakka_version              */
   NULL,   /* set_screen_brightness          */
   NULL,   /* watch_path_for_changes         */
   NULL,   /* check_for_path_changes         */
   NULL,   /* set_sustained_performance_mode */
   NULL,   /* get_cpu_model_name             */
   NULL,   /* get_user_language              */
   NULL,   /* is_narrator_running            */
   NULL,   /* accessibility_speak            */
   NULL,   /* set_gamemode                   */
   "wiiu", /* ident                          */
   NULL    /* get_video_driver               */
};

/* main() and its supporting functions */

static void main_setup(void);
#ifndef IS_SALAMANDER
static void main_loop(void);
#endif
static void main_teardown(void);

static void    init_logging(void);
static void    deinit_logging(void);
static void    init_filesystems(void);
static void    deinit_filesystems(void);
static ssize_t wiiu_log_write(struct _reent* r, void* fd, const char* ptr, size_t len);
static void    init_pad_libraries(void);
static void    deinit_pad_libraries(void);
static void    proc_setup(void);
static void    proc_exit(void);
static void    proc_save_callback(void);

int main(int argc, char** argv)
{
   proc_setup();
   main_setup();

#ifdef IS_SALAMANDER
   int salamander_main(int argc, char** argv);
   salamander_main(argc, argv);
#else
   rarch_main(argc, argv, NULL);
   main_loop();
   main_exit(NULL);
#endif /* IS_SALAMANDER */
   main_teardown();

   proc_exit();
   /* We always return 0 because if we don't, it can prevent loading a
    * different RPX/ELF in HBL. */
   return 0;
}

static void main_setup(void)
{
   init_os_exceptions();
   init_logging();
   init_filesystems();
   init_pad_libraries();
   verbosity_enable();
   fflush(stdout);
}

static void main_teardown(void)
{
   deinit_pad_libraries();
   deinit_filesystems();
   deinit_logging();
   deinit_os_exceptions();
}

static void proc_setup(void)
{
   OSDynLoad_Module rpx_module;
   // Explicitly detect Aroma
   if (OSDynLoad_Acquire("homebrew_rpx_loader", &rpx_module) == OS_DYNLOAD_OK)
   {
      in_aroma = true;
      OSDynLoad_Release(rpx_module);
   }

   ProcUIInit(&proc_save_callback);
}

static void proc_exit(void)
{
   ProcUIShutdown();
}

static void proc_save_callback(void)
{
   OSSavesDone_ReadyToRelease();
}

#ifndef IS_SALAMANDER
static bool swap_is_pending(void* start_time)
{
   uint32_t swap_count, flip_count;
   OSTime   last_flip,  last_vsync;

   GX2GetSwapStatus(&swap_count, &flip_count, &last_flip, &last_vsync);
   return last_vsync < *(OSTime*)start_time;
}

static void main_loop(void)
{
   OSTime start_time;
   int    status;

   for (;;)
   {
      if (video_driver_get_ptr())
      {
         start_time = OSGetSystemTime();
         task_queue_wait(swap_is_pending, &start_time);
      }
      else
         task_queue_wait(NULL, NULL);

      status = runloop_iterate();

      if (status == -1)
         break;
   }
}
#endif

static devoptab_t dotab_stdout =
{
   "stdout_whb",   /* device name */
   0,              /* size of file structure */
   NULL,           /* device open */
   NULL,           /* device close */
   wiiu_log_write, /* device write */
   NULL,           /* ... */
};

static void init_logging(void)
{
   if (!WHBLogModuleInit())
   {
      WHBLogUdpInit();
      WHBLogCafeInit();
   }

   devoptab_list[STD_OUT] = &dotab_stdout;
   devoptab_list[STD_ERR] = &dotab_stdout;
}

static void deinit_logging(void)
{
   fflush(stdout);
   fflush(stderr);

   WHBLogModuleDeinit();
   WHBLogUdpDeinit();
   WHBLogCafeDeinit();
}

static ssize_t wiiu_log_write(struct _reent* r,
                              void*          fd, const char* ptr, size_t len)
{
   // Do a bit of line buffering to try and make the log output nicer
   // we just truncate if a line goes over
   static char   linebuf[2048]; //match wut's PRINTF_BUFFER_LENGTH
   static size_t linebuf_pos = 0;

   snprintf(linebuf + linebuf_pos, sizeof(linebuf) - linebuf_pos - 1, "%.*s",
            len, ptr);
   linebuf_pos = strlen(linebuf);

   if (linebuf[linebuf_pos - 1] == '\n' || linebuf_pos >= sizeof(linebuf) - 2)
   {
      WHBLogWrite(linebuf);
      linebuf_pos = 0;
   }

   return (ssize_t)len;
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

static void init_filesystems(void)
{
#if defined(HAVE_LIBMOCHA)
   MochaUtilsStatus mocha_res;

   mocha_res = Mocha_InitLibrary();
   DEBUG_VAR(mocha_res);
   if (mocha_res != MOCHA_RESULT_SUCCESS)
      return;

   Mocha_MountFS("storage_usb", NULL, "/vol/storage_usb01");
   have_wfs_usb = exists(WIIU_STORAGE_USB_PATH);
   DEBUG_VAR(have_wfs_usb);

#if defined(HAVE_LIBFAT)
   if (!in_aroma)
      have_libfat_sdcard = fatMountSimple("sd", &Mocha_sdio_disc_interface);
   else
      Mocha_MountFS("sd", NULL, "/vol/external01");
   DEBUG_VAR(have_libfat_sdcard);

   have_libfat_usb = fatMountSimple("usb", &Mocha_usb_disc_interface);
   DEBUG_VAR(have_libfat_usb);
#else
   Mocha_MountFS("sd", NULL, "/vol/external01");
#endif
#endif
}

static void deinit_filesystems(void)
{
#if defined(HAVE_LIBMOCHA)
   if (have_wfs_usb)
      Mocha_UnmountFS("usb");

#if defined(HAVE_LIBFAT)
   if (have_libfat_usb)
      fatUnmount("usb");
   if (have_libfat_sdcard)
      fatUnmount("sd");
   else if (in_aroma)
      Mocha_UnmountFS("sd");

   Mocha_sdio_disc_interface.shutdown();
   Mocha_usb_disc_interface.shutdown();
#else
   Mocha_UnmountFS("sd");
#endif

   Mocha_DeInitLibrary();
#endif
}

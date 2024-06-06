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

#include "../configuration.h"
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
#include <coreinit/memory.h>
#include <proc_ui/procui.h>
#include <proc_ui/memory.h>
#include <sysapp/launch.h>
#include <gx2/event.h>
#include <coreinit/foreground.h>
#include <padscore/kpad.h>
#include <whb/log.h>
#include <whb/log_cafe.h>
#include <whb/log_module.h>
#include <whb/log_udp.h>

#define WIIU_SD_PATH "sd:/"
#define WIIU_VOL_CONTENT_PATH "fs:/vol/content/"
#define WIIU_USB_PATH "usb:/"
#define WIIU_STORAGE_USB_PATH "storage_usb:/"

/**
 * The Wii U frontend driver, along with the main() method.
 */

#ifndef IS_SALAMANDER
static enum frontend_fork wiiu_fork_mode     = FRONTEND_FORK_NONE;
static bool               have_libfat_usb    = false;
static bool               have_libfat_sdcard = false;
static bool               have_wfs_usb       = false;
#endif
static bool in_exec = false;

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

static bool vol_content_assets_exist(void)
{
   char path_buf[PATH_MAX_LENGTH] = {0};

   fill_pathname_join(path_buf, WIIU_VOL_CONTENT_PATH, "assets", sizeof(path_buf));

   return exists(path_buf);
}

static void frontend_wiiu_get_env_settings(int*  argc, char* argv[],
                                           void* args, void* params_data)
{
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PORT], WIIU_SD_PATH,
                      "retroarch", sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fix_asset_directory();
   if (vol_content_assets_exist())
      fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], WIIU_VOL_CONTENT_PATH,
                         "assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   else
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

static void frontend_wiiu_exec(const char* path, bool should_load_content)
{
   /* goal: make one big buffer with all the argv's, seperated by NUL. we can
    * then pass this thru sysapp! */
   char*  argv_buf;
   size_t n, argv_len = strlen(path) + 1; /* argv[0] plus null */

#ifndef IS_SALAMANDER
   const char* content         = path_get(RARCH_PATH_CONTENT);
   const char* content_args[2] = {content, NULL};
#ifdef HAVE_NETWORKING
   const char* netplay_args[NETPLAY_FORK_MAX_ARGS];
#endif
#endif
   /* args will select between content_args, netplay_args, or no args (default) */
   const char** args = NULL;

   /* and some other stuff (C89) */
   MochaRPXLoadInfo  load_info = {0};
   MochaUtilsStatus  ret;
   SYSStandardArgsIn std_args = {0};

#ifndef IS_SALAMANDER
   if (should_load_content)
   {
#ifdef HAVE_NETWORKING
      if (netplay_driver_ctl(RARCH_NETPLAY_CTL_GET_FORK_ARGS,
                             (void*)netplay_args))
      {
         const char** cur_arg = netplay_args;

         do
            argv_len += strnlen(*cur_arg, PATH_MAX_LENGTH) + 1;
         while (*(++cur_arg));

         args = netplay_args;
      }
      else
#endif
         if (!string_is_empty(content))
         {
            argv_len += strnlen(content, PATH_MAX_LENGTH) + 1;
            args = content_args;
         }
   }
#endif

   argv_buf    = malloc(argv_len);
   argv_buf[0] = '\0';

   n = strlcpy(argv_buf, path, argv_len);
   n++; /* leave room for the NUL */
   if (args)
   {
      const char** cur_arg = args;
      do
      {
         n += strlcpy(argv_buf + n, *cur_arg, argv_len - n);
         n++;
      }
      while (*(++cur_arg));
   }

   if (string_starts_with(path, "fs:/vol/external01/"))
      path_relative_to(load_info.path, path, "fs:/vol/external01/", sizeof(load_info.path));
   else if (string_starts_with(path, "sd:/"))
      path_relative_to(load_info.path, path, "sd:/", sizeof(load_info.path));
   else goto cleanup; /* bail if not on the SD card */

   /* Mocha might not be init'd (Salamander) */
   if (Mocha_InitLibrary() != MOCHA_RESULT_SUCCESS)
      goto cleanup;

   load_info.target = LOAD_RPX_TARGET_SD_CARD;
   ret              = Mocha_PrepareRPXLaunch(&load_info);
   if (ret != MOCHA_RESULT_SUCCESS)
      goto cleanup;

   std_args.argString = argv_buf;
   std_args.size      = argv_len;
   ret                = Mocha_LaunchHomebrewWrapperEx(&std_args);
   if (ret != MOCHA_RESULT_SUCCESS)
   {
      MochaRPXLoadInfo load_info_revert;
      load_info_revert.target = LOAD_RPX_TARGET_EXTRA_REVERT_PREPARE;
      Mocha_PrepareRPXLaunch(&load_info_revert);
      goto cleanup;
   }

   in_exec = true;

cleanup:
   free(argv_buf);
   argv_buf = NULL;
}

/* ProcUI lifetime: this gets called from main_exit/salamander_main, which we call from main
 * This is BEFORE the final shutdown loop so we can just exec cores and it'll handle the mess */
#ifdef IS_SALAMANDER
static void frontend_wiiu_exitspawn(char* s, size_t len, char* args)
{
   frontend_wiiu_exec(s, false);
}
#else /* ifndef IS_SALAMANDER */
static void frontend_wiiu_exitspawn(char* s, size_t len, char* args)
{
   if (wiiu_fork_mode != FRONTEND_FORK_NONE)
   {
      /* Load a core */
      bool should_load_content = wiiu_fork_mode == FRONTEND_FORK_CORE_WITH_ARGS;
      frontend_wiiu_exec(s, should_load_content);
   }
}
#endif /* IS_SALAMANDER */

#ifndef IS_SALAMANDER
static bool frontend_wiiu_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
   case FRONTEND_FORK_CORE:
   case FRONTEND_FORK_CORE_WITH_ARGS:
      wiiu_fork_mode = fork_mode;
      break;
   case FRONTEND_FORK_RESTART:
      /* NOTE: We don't implement Salamander, so just turn
       * this into FRONTEND_FORK_CORE. */
      wiiu_fork_mode = FRONTEND_FORK_CORE;
      break;
   case FRONTEND_FORK_NONE:
   default:
      return false;
   }

   return true;
}
#endif

frontend_ctx_driver_t frontend_ctx_wiiu =
{
   frontend_wiiu_get_env_settings,
   frontend_wiiu_init,
   frontend_wiiu_deinit,
   frontend_wiiu_exitspawn,
   NULL, /* process_args */
   frontend_wiiu_exec,
#ifdef IS_SALAMANDER
   NULL, /* set_fork */
#else
   frontend_wiiu_set_fork,
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
static void get_arguments(int* argc, char*** argv);
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
   get_arguments(&argc, &argv);

#ifdef IS_SALAMANDER
   int salamander_main(int argc, char** argv);
   salamander_main(argc, argv);
#else
   rarch_main(argc, argv, NULL);
   main_loop();
   main_exit(NULL);
#endif /* IS_SALAMANDER */

   if (!ProcUIInShutdown())
   {
      /* Final proc loop to negotiate shutdown */
      ProcUIStatus os_status;
      while ((os_status = ProcUIProcessMessages(TRUE)) != PROCUI_STATUS_EXITING)
      {
         /* If nobody has requested a new app already (e.g. a core switch), we'll still be in foreground
          * We should exit to menu */
         if (os_status == PROCUI_STATUS_IN_FOREGROUND)
            SYSLaunchMenu();
            /* Core switch puts us here, we're good to release */
         else if (os_status == PROCUI_STATUS_RELEASE_FOREGROUND)
            ProcUIDrawDoneRelease();
         else
            DEBUG_LINE(); /* BUG */
      }
   }

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

static bool  in_aroma             = false;
static void* procui_mem1Storage   = NULL;
static void* procui_bucketStorage = NULL;

static uint32_t proc_acquired(void* param)
{
   (void)param;
   init_os_exceptions();
   return 0;
}

static uint32_t proc_release(void* param)
{
   (void)param;
   deinit_os_exceptions();
   return 0;
}

static bool in_main = false;

static uint32_t proc_home_button_deny(void* param)
{
   (void)param;

   /* Don't toggle the menu in, like, the middle of a core switch */
   if (in_main)
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);

   return 0;
}

static void proc_setup(void)
{
   /* Detect Aroma explicitly (it's possible to run under H&S while using Tiramisu) */
   OSDynLoad_Module rpx_module;
   if (OSDynLoad_Acquire("homebrew_rpx_loader", &rpx_module) == OS_DYNLOAD_OK)
   {
      in_aroma = true;
      OSDynLoad_Release(rpx_module);
   }

   ProcUIInit(&proc_save_callback);
   ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, proc_acquired, NULL, 1);
   ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, proc_release, NULL, 1);
   ProcUIRegisterCallback(PROCUI_CALLBACK_HOME_BUTTON_DENIED, &proc_home_button_deny, NULL, 1000);

   uint32_t addr = 0;
   uint32_t size = 0;
   if (OSGetMemBound(OS_MEM1, &addr, &size) == 0)
   {
      procui_mem1Storage = malloc(size);
      if (procui_mem1Storage)
      {
         ProcUISetMEM1Storage(procui_mem1Storage, size);
      }
   }
   if (OSGetForegroundBucketFreeArea(&addr, &size))
   {
      procui_bucketStorage = malloc(size);
      if (procui_bucketStorage)
      {
         ProcUISetBucketStorage(procui_bucketStorage, size);
      }
   }
}

static void proc_exit(void)
{
   if (procui_mem1Storage)
   {
      free(procui_mem1Storage);
      procui_mem1Storage = NULL;
   }
   if (procui_bucketStorage)
   {
      free(procui_bucketStorage);
      procui_bucketStorage = NULL;
   }
   ProcUIShutdown();
}

static void proc_save_callback(void)
{
   OSSavesDone_ReadyToRelease();
}

static void sysapp_arg_cb(SYSDeserializeArg* arg, void* usr)
{
   SYSStandardArgs* std_args = (SYSStandardArgs*)usr;

   if (_SYSDeserializeStandardArg(arg, std_args))
      return;

   if (strcmp(arg->argName, "sys:pack") == 0)
      SYSDeserializeSysArgsFromBlock(arg->data, arg->size, sysapp_arg_cb, usr);
}

static void get_arguments(int* argc, char*** argv)
{
#ifdef HAVE_NETWORKING
   static char* _argv[1 + NETPLAY_FORK_MAX_ARGS];
#else
   static char* _argv[2];
#endif
   int             _argc    = 0;
   SYSStandardArgs std_args = {0};

   /* we could do something more rich with the content path and things here -
    * but since there's not a great way to actually pass that info along to RA,
    * just emulate argc/argv */
   SYSDeserializeSysArgs(sysapp_arg_cb, &std_args);

   char*  argv_buf = std_args.anchorData;
   size_t argv_len = std_args.anchorSize;
   if (!argv_buf || argv_len == 0)
      return;

   size_t n = 0;
   while (n < argv_len && _argc < ARRAY_SIZE(_argv))
   {
      char* s        = argv_buf + n;
      _argv[_argc++] = s;
      n += strlen(s);
      n++; /* skip the null */
   }

   *argc = _argc;
   *argv = _argv;
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
   OSTime       start_time;
   ProcUIStatus os_status;
   int          status;
   settings_t*  settings = config_get_ptr();

   in_main = true;

   while ((os_status = ProcUIProcessMessages(TRUE)) != PROCUI_STATUS_EXITING)
   {
      if (os_status == PROCUI_STATUS_IN_BACKGROUND) continue;

      if (os_status == PROCUI_STATUS_IN_FOREGROUND)
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
         {
            /* RetroArch requested core switch or exit */
            break;
         }
      }
      else if (os_status == PROCUI_STATUS_RELEASE_FOREGROUND)
      {
         /* Home menu opened - also open RA menu
          * This could also be like, an OS poweroff, but it doesn't really matter if we open the menu for 1 frame in
          * that case*/
         if (!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
         {
            command_event(CMD_EVENT_MENU_TOGGLE, NULL);
         }
         ProcUIDrawDoneRelease();
      }

      if (OSIsHomeButtonMenuEnabled() != settings->bools.input_wiiu_enable_hbm)
         OSEnableHomeButtonMenu(settings->bools.input_wiiu_enable_hbm);
   }

   in_main = false;
   OSEnableHomeButtonMenu(FALSE);
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
   /* Do a bit of line buffering to try and make the log output nicer
    * we just truncate if a line goes over */
   static char   linebuf[2048]; /* match wut's PRINTF_BUFFER_LENGTH */
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

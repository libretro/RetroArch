/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef VITA
#include <psp2/system_param.h>
#include <psp2/power.h>
#include <psp2/sysmodule.h>
#include <psp2/appmgr.h>
#include <psp2/apputil.h>

#include "../../bootstrap/vita/sbrk.c"

#else
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspfpu.h>
#include <psppower.h>
#include <pspsdk.h>
#endif

#include <pthread.h>

#include <string/stdstring.h>
#include <boolean.h>
#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../frontend_driver.h"
#include "../../defaults.h"
#include "../../file_path_special.h"
#include <defines/psp_defines.h>
#include "../../retroarch.h"
#include "../../paths.h"
#include "../../verbosity.h"

#if !defined(IS_SALAMANDER) && defined(HAVE_NETWORKING)
#include "../../network/netplay/netplay.h"
#endif

#if defined(PSP) && defined(HAVE_KERNEL_PRX)
#include "../../bootstrap/psp1/kernel_functions.h"
#endif

#if defined(HAVE_VITAGLES)
#include "../../deps/Pigs-In-A-Blanket/include/pib.h"
#endif

#ifndef VITA
PSP_MODULE_INFO("RetroArch", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER|THREAD_ATTR_VFPU);
#endif

#ifdef SCE_LIBC_SIZE
unsigned int sceLibcHeapSize = SCE_LIBC_SIZE;
#endif

char eboot_path[512];
char user_path[512];

static enum frontend_fork psp_fork_mode = FRONTEND_FORK_NONE;

static void frontend_psp_get_env_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
#ifndef IS_SALAMANDER
   struct rarch_main_wrap *params = (struct rarch_main_wrap*)params_data;
#endif

#ifdef VITA
   strlcpy(eboot_path, "app0:/", sizeof(eboot_path));
   strlcpy(user_path, "ux0:/data/retroarch/", sizeof(user_path));

   strlcpy(g_defaults.dirs[DEFAULT_DIR_PORT], eboot_path,
      sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   /* bundle data*/
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE],
      g_defaults.dirs[DEFAULT_DIR_PORT], "",
      sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
      g_defaults.dirs[DEFAULT_DIR_PORT], "info",
      sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   /* user data*/
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], user_path, "assets",
      sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], user_path,
      "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], user_path,
      "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], user_path, "cheats",
      sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], user_path,
      "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], user_path,
      "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], user_path,
      "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], user_path, "remaps",
      sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], user_path,
      "savefiles", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], user_path,
      "savestates", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], user_path, "system",
      sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CACHE], user_path, "temp",
      sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], user_path,
      "overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT], user_path,
      "layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], user_path,
      "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], user_path, "logs",
      sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY], user_path,
      sizeof(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]));
   fill_pathname_join(g_defaults.path_config, user_path, FILE_PATH_MAIN_CONFIG,
      sizeof(g_defaults.path_config));
#else
   strlcpy(eboot_path, argv[0], sizeof(eboot_path));
   /* for PSP, use uppercase directories, and no trailing slashes
      otherwise mkdir fails */
   strlcpy(user_path, "ms0:/PSP/RETROARCH", sizeof(user_path));

   fill_pathname_basedir(g_defaults.dirs[DEFAULT_DIR_PORT], argv[0],
      sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   /* bundle data*/
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE],
      g_defaults.dirs[DEFAULT_DIR_PORT], "CORES",
      sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
      g_defaults.dirs[DEFAULT_DIR_PORT], "INFO",
      sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   /* user data */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], user_path, "CHEATS",
      sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], user_path,
      "CONFIG", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], user_path,
      "DOWNLOADS", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], user_path,
      "PLAYLISTS", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP],
      g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], "REMAPS",
      sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], user_path,
      "SAVEFILES", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], user_path,
      "SAVESTATES", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], user_path,
      "SCREENSHOTS", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], user_path, "SYSTEM",
      sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], user_path, "LOGS",
      sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CACHE], user_path, "TEMP",
      sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY], user_path,
      sizeof(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]));
   fill_pathname_join(g_defaults.path_config, user_path, FILE_PATH_MAIN_CONFIG,
      sizeof(g_defaults.path_config));
#endif

#ifndef IS_SALAMANDER
   if (params && *argc > 1 && !string_is_empty(argv[1]))
#ifdef HAVE_NETWORKING
   /* If the process was forked for netplay purposes,
      DO NOT touch the arguments. */
   if (!string_is_equal(argv[1], "-H") && !string_is_equal(argv[1], "-C"))
#endif
   {
      params->content_path  = argv[1];
      params->sram_path     = NULL;
      params->state_path    = NULL;
      params->config_path   = NULL;
      params->libretro_path = NULL;
      params->verbose       = false;
      params->no_content    = false;
      params->touched       = true;
   }

   dir_check_defaults("custom.ini");
#endif
}

static void frontend_psp_deinit(void *data)
{
   (void)data;
#ifndef IS_SALAMANDER
   pthread_terminate();
#endif
}

static void frontend_psp_shutdown(bool unused)
{
   (void)unused;
#ifdef VITA
   //sceKernelExitProcess(0);
   return;
#else
   exit(0);
#endif
}

#ifndef VITA
static int exit_callback(int arg1, int arg2, void *common)
{
   frontend_psp_deinit(NULL);
   frontend_psp_shutdown(false);
   return 0;
}

static int callback_thread(SceSize args, void *argp)
{
   int cbid = sceKernelCreateCallback("Exit callback", exit_callback, NULL);
   sceKernelRegisterExitCallback(cbid);
   sceKernelSleepThreadCB();

   return 0;
}

static int setup_callback(void)
{
   int thread_id = sceKernelCreateThread("update_thread",
         callback_thread, 0x11, 0xFA0, 0, 0);

   if (thread_id >= 0)
      sceKernelStartThread(thread_id, 0, 0);

   return thread_id;
}
#endif

static void frontend_psp_init(void *data)
{
#ifndef IS_SALAMANDER

#ifdef VITA
   scePowerSetArmClockFrequency(444);
   scePowerSetBusClockFrequency(222);
   scePowerSetGpuClockFrequency(222);
   scePowerSetGpuXbarClockFrequency(166);
   sceSysmoduleLoadModule(SCE_SYSMODULE_NET);

   SceAppUtilInitParam appUtilParam;
   SceAppUtilBootParam appUtilBootParam;
   memset(&appUtilParam, 0, sizeof(SceAppUtilInitParam));
   memset(&appUtilBootParam, 0, sizeof(SceAppUtilBootParam));
   sceAppUtilInit(&appUtilParam, &appUtilBootParam);
#if defined(HAVE_VITAGLES)
   if(pibInit(PIB_SHACCCG|PIB_ENABLE_MSAA|PIB_GET_PROC_ADDR_CORE))
      return;
#endif
#else
   (void)data;
   /* initialize debug screen */
   pspDebugScreenInit();
   pspDebugScreenClear();

   setup_callback();

   pspFpuSetEnable(0); /* disable FPU exceptions */
   scePowerSetClockFrequency(333,333,166);
#endif
   pthread_init();

#endif

#if defined(PSP) && defined(HAVE_KERNEL_PRX) 
   pspSdkLoadStartModule("kernel_functions.prx", PSP_MEMORY_PARTITION_KERNEL);
#endif
}

static void frontend_psp_exec(const char *path, bool should_load_game)
{
#if defined(PSP)
#ifdef HAVE_KERNEL_PRX
   char    argp[PATH_MAX_LENGTH];
   SceSize args = strlcpy(argp, eboot_path, sizeof(argp)) + 1;

#ifndef IS_SALAMANDER
   if (args < sizeof(argp) && should_load_game)
   {
      const char *content = path_get(RARCH_PATH_CONTENT);

      if (!string_is_empty(content))
         args += strlcpy(argp + args, content, sizeof(argp) - args) + 1;
   }
#endif

   if (args > sizeof(argp))
      args = sizeof(argp);

   exitspawn_kernel(path, args, argp);
#endif
#elif defined(VITA)
#ifndef IS_SALAMANDER
#ifdef HAVE_NETWORKING
   char *arg_data[NETPLAY_FORK_MAX_ARGS];
#else
   char *arg_data[2];
#endif
   char game_path[PATH_MAX_LENGTH];
#else
   char *arg_data[2];
   char boot_params[PATH_MAX_LENGTH];
#endif

   arg_data[0] = NULL;

#ifndef IS_SALAMANDER
   if (should_load_game)
   {
      const char *content = path_get(RARCH_PATH_CONTENT);

#ifdef HAVE_NETWORKING
      if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_GET_FORK_ARGS,
            (void*)arg_data))
#endif
      if (!string_is_empty(content))
      {
         strlcpy(game_path, content, sizeof(game_path));
         arg_data[0] = game_path;
         arg_data[1] = NULL;
      }
   }
#else
   sceAppMgrGetAppParam(boot_params);
   if (strstr(boot_params, "psgm:play"))
   {
      char *param1 = strstr(boot_params, "&param=");
      char *param2 = strstr(boot_params, "&param2=");

      if (param1 && param2)
      {
         char *delim;

         param1 += STRLEN_CONST("&param=");
         param2 += STRLEN_CONST("&param2=");

         delim = strchr(param1, '&');
         if (delim)
            *delim = '\0';
         delim = strchr(param2, '&');
         if (delim)
            *delim = '\0';

         if (!string_is_empty(param1))
         {
            path = param1;

            if (!string_is_empty(param2))
            {
               arg_data[0] = param2;
               arg_data[1] = NULL;
            }
         }
      }
   }
#endif
   sceAppMgrLoadExec(path, arg_data[0] ? arg_data : NULL, NULL);
#endif
}

#ifndef IS_SALAMANDER
static bool frontend_psp_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         psp_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         psp_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         /* NOTE: We don't implement Salamander, so just turn
          * this into FRONTEND_FORK_CORE. */
         psp_fork_mode  = FRONTEND_FORK_CORE;
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}
#endif

static void frontend_psp_exitspawn(char *s, size_t len, char *args)
{
   bool should_load_content = false;
#ifndef IS_SALAMANDER
   if (psp_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (psp_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_content = true;
         break;
      case FRONTEND_FORK_NONE:
      default:
         break;
   }
#endif
   frontend_psp_exec(s, should_load_content);
}

static int frontend_psp_get_rating(void)
{
#ifdef VITA
   return 6; /* Go with a conservative figure for now. */
#else
   return 4;
#endif
}

static enum frontend_powerstate frontend_psp_get_powerstate(int *seconds, int *percent)
{
   enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;
#ifndef VITA
   int battery                  = scePowerIsBatteryExist();
#endif
   int plugged                  = scePowerIsPowerOnline();
   int charging                 = scePowerIsBatteryCharging();

   *percent = scePowerGetBatteryLifePercent();
   *seconds = scePowerGetBatteryLifeTime() * 60;

#ifndef VITA
   if (!battery)
   {
      ret = FRONTEND_POWERSTATE_NO_SOURCE;
      *seconds = -1;
      *percent = -1;
   }
   else
#endif
   if (charging)
      ret = FRONTEND_POWERSTATE_CHARGING;
   else if (plugged)
      ret = FRONTEND_POWERSTATE_CHARGED;
   else
      ret = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   return ret;
}

enum frontend_architecture frontend_psp_get_arch(void)
{
#ifdef VITA
   return FRONTEND_ARCH_ARMV7;
#else
   return FRONTEND_ARCH_MIPS;
#endif
}

static int frontend_psp_parse_drive_list(void *data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content ?
      MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
      MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;

#ifdef VITA
   menu_entries_append(list,
         "app0:/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "ur0:/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "ux0:/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "uma0:/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "imc0:/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
#else
   menu_entries_append(list,
         "ms0:/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "ef0:/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
   menu_entries_append(list,
         "host0:/",
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0, NULL);
#endif
#endif

   return 0;
}

#ifdef VITA
enum retro_language psp_get_retro_lang_from_langid(int langid)
{
   switch (langid)
   {
   case SCE_SYSTEM_PARAM_LANG_JAPANESE:
      return RETRO_LANGUAGE_JAPANESE;
   case SCE_SYSTEM_PARAM_LANG_FRENCH:
      return RETRO_LANGUAGE_FRENCH;
   case SCE_SYSTEM_PARAM_LANG_SPANISH:
      return RETRO_LANGUAGE_SPANISH;
   case SCE_SYSTEM_PARAM_LANG_GERMAN:
      return RETRO_LANGUAGE_GERMAN;
   case SCE_SYSTEM_PARAM_LANG_ITALIAN:
      return RETRO_LANGUAGE_ITALIAN;
   case SCE_SYSTEM_PARAM_LANG_DUTCH:
      return RETRO_LANGUAGE_DUTCH;
   case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT:
      return RETRO_LANGUAGE_PORTUGUESE_PORTUGAL;
   case SCE_SYSTEM_PARAM_LANG_RUSSIAN:
      return RETRO_LANGUAGE_RUSSIAN;
   case SCE_SYSTEM_PARAM_LANG_KOREAN:
      return RETRO_LANGUAGE_KOREAN;
   case SCE_SYSTEM_PARAM_LANG_CHINESE_T:
      return RETRO_LANGUAGE_CHINESE_TRADITIONAL;
   case SCE_SYSTEM_PARAM_LANG_CHINESE_S:
      return RETRO_LANGUAGE_CHINESE_SIMPLIFIED;
   case SCE_SYSTEM_PARAM_LANG_POLISH:
      return RETRO_LANGUAGE_POLISH;
   case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR:
      return RETRO_LANGUAGE_PORTUGUESE_BRAZIL;
   case SCE_SYSTEM_PARAM_LANG_TURKISH:
      return RETRO_LANGUAGE_TURKISH;
#if 0
   /* TODO/FIXME - this doesn't seem to actually exist */
   case SCE_SYSTEM_PARAM_LANG_SLOVAK:
      return RETRO_LANGUAGE_SLOVAK;
#endif
   case SCE_SYSTEM_PARAM_LANG_ENGLISH_US:
   case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB:
   default:
      return RETRO_LANGUAGE_ENGLISH;
   }
}

enum retro_language frontend_psp_get_user_language(void)
{
   int langid;
   sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &langid);
   return psp_get_retro_lang_from_langid(langid);
}

static uint64_t frontend_psp_get_total_mem(void)
{
   return _newlib_heap_end - _newlib_heap_base;
}

static uint64_t frontend_psp_get_free_mem(void)
{
   return _newlib_heap_end - _newlib_heap_cur;
}
#endif

frontend_ctx_driver_t frontend_ctx_psp = {
   frontend_psp_get_env_settings,/* get_env_settings */
   frontend_psp_init,            /* init             */
   frontend_psp_deinit,          /* deinit           */
   frontend_psp_exitspawn,       /* exitspawn        */
   NULL,                         /* process_args     */
   frontend_psp_exec,            /* exec             */
#ifdef IS_SALAMANDER
   NULL,                         /* set_fork         */
#else
   frontend_psp_set_fork,        /* set_fork         */
#endif
   frontend_psp_shutdown,        /* shutdown         */
   NULL,                         /* get_name         */
   NULL,                         /* get_os           */
   frontend_psp_get_rating,      /* get_rating       */
   NULL,                         /* content_loaded   */
   frontend_psp_get_arch,        /* get_architecture */
   frontend_psp_get_powerstate,
   frontend_psp_parse_drive_list,
#ifdef VITA
   frontend_psp_get_total_mem,
   frontend_psp_get_free_mem,
#else
   NULL,                         /* get_total_mem    */
   NULL,                         /* get_free_mem     */
#endif
   NULL,                         /* install_signal_handler */
   NULL,                         /* get_sighandler_state */
   NULL,                         /* set_sighandler_state */
   NULL,                         /* destroy_sighandler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   NULL,                         /* get_lakka_version */
   NULL,                         /* set_screen_brightness */
   NULL,                         /* watch_path_for_changes */
   NULL,                         /* check_for_path_changes */
   NULL,                         /* set_sustained_performance_mode */
   NULL,                         /* get_cpu_model_name */
#ifdef VITA
   frontend_psp_get_user_language, /* get_user_language */
   NULL,                         /* is_narrator_running */
   NULL,                         /* accessibility_speak */
   NULL,                         /* set_gamemode */
   "vita",                       /* ident */
#else
   NULL,                         /* get_user_language */
   NULL,                         /* is_narrator_running */
   NULL,                         /* accessibility_speak */
   NULL,                         /* set_gamemode */
   "psp",                        /* ident */
#endif
   NULL                          /* get_video_driver */
};

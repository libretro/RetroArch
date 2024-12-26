/* RetroArch - A frontend for libretro.
 * Copyright (C) 2011-2017 - Daniel De Matteis
 * Copyright (C) 2016-2019 - Brad Parker
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

#include <retro_miscellaneous.h>
#include <windows.h>

#include <boolean.h>
#include <compat/strl.h>
#include <dynamic/dylib.h>
#include <lists/file_list.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../frontend_driver.h"

#include "../../configuration.h"
#include "../../defaults.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../ui/drivers/ui_win32.h"

#include "../../uwp/uwp_func.h"

static void frontend_uwp_get_os(char *s, size_t len, int *major, int *minor)
{
   size_t _len;
   char build_str[11]     = {0};
   bool server            = false;
   const char *arch       = "";
   SYSTEM_INFO si         = {{0}};
   OSVERSIONINFOEX vi     = {0};
   vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   GetSystemInfo(&si);

   GetVersionEx((OSVERSIONINFO*)&vi);

   server = vi.wProductType != VER_NT_WORKSTATION;

   switch (si.wProcessorArchitecture)
   {
      case PROCESSOR_ARCHITECTURE_AMD64:
         arch = "x64";
         break;
      case PROCESSOR_ARCHITECTURE_INTEL:
         arch = "x86";
         break;
      case PROCESSOR_ARCHITECTURE_ARM:
         arch = "ARM";
         break;
      case PROCESSOR_ARCHITECTURE_ARM64:
         arch = "ARM64";
         break;
      default:
         break;
   }

   if (major)
      *major = vi.dwMajorVersion;

   if (minor)
      *minor = vi.dwMinorVersion;

   snprintf(build_str, sizeof(build_str), "%lu", vi.dwBuildNumber);

   switch (vi.dwMajorVersion)
   {
      case 10:
         if (server)
         {
            if ((vi.dwBuildNumber >= 14393) && (vi.dwBuildNumber < 17763))
               _len = strlcpy(s, "Windows Server 2016", len);
            else if ((vi.dwBuildNumber >= 17763) && (vi.dwBuildNumber < 20348))
               _len = strlcpy(s, "Windows Server 2019", len);
            else if (vi.dwBuildNumber >= 20348)
               _len = strlcpy(s, "Windows Server 2022", len);
         }
         else
         {
            if ((vi.dwBuildNumber >= 10240) && (vi.dwBuildNumber < 22000))
               _len = strlcpy(s, "Windows 10", len);
            else if (vi.dwBuildNumber >= 22000)
               _len = strlcpy(s, "Windows 11", len);
         }
         break;
      default:
         _len = snprintf(s, len, "Windows %i.%i", *major, *minor);
         break;
   }

   if (!string_is_empty(arch))
   {
      _len += strlcpy(s + _len, " ",  len - _len);
      _len += strlcpy(s + _len, arch, len - _len);
   }

   _len += strlcpy(s + _len, " Build ", len - _len);
   _len += strlcpy(s + _len, build_str, len - _len);

   if (!string_is_empty(vi.szCSDVersion))
   {
      _len += strlcpy(s + _len, " ", len - _len);
      _len += strlcpy(s + _len, vi.szCSDVersion, len - _len);
   }

   if (!string_is_empty(uwp_device_family))
   {
      _len += strlcpy(s + _len, " ", len - _len);
      strlcpy(s + _len, uwp_device_family, len - _len);
   }
}

static void frontend_uwp_init(void *data) { }

enum frontend_powerstate frontend_uwp_get_powerstate(
      int *seconds, int *percent)
{
   SYSTEM_POWER_STATUS status;
   enum frontend_powerstate
      ret         = FRONTEND_POWERSTATE_NONE;

   if (GetSystemPowerStatus(&status))
   {
      if (status.BatteryFlag == 0xFF)
         ret      = FRONTEND_POWERSTATE_NONE;
      else if (status.BatteryFlag & (1 << 7))
         ret      = FRONTEND_POWERSTATE_NO_SOURCE;
      else if (status.BatteryFlag & (1 << 3))
         ret      = FRONTEND_POWERSTATE_CHARGING;
      else if (status.ACLineStatus == 1)
         ret      = FRONTEND_POWERSTATE_CHARGED;
      else
         ret      = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

      *percent    = (int)status.BatteryLifePercent;
      *seconds    = (int)status.BatteryLifeTime;

#ifdef _WIN32
      if (*percent == 255)
         *percent = 0;
#endif
   }

   return ret;
}

enum frontend_architecture frontend_uwp_get_arch(void)
{
   SYSTEM_INFO si = {{0}};

   GetSystemInfo(&si);

   switch (si.wProcessorArchitecture)
   {
      case PROCESSOR_ARCHITECTURE_AMD64:
         return FRONTEND_ARCH_X86_64;
      case PROCESSOR_ARCHITECTURE_INTEL:
         return FRONTEND_ARCH_X86;
      case PROCESSOR_ARCHITECTURE_ARM:
         return FRONTEND_ARCH_ARM;
      case PROCESSOR_ARCHITECTURE_ARM64:
         return FRONTEND_ARCH_ARMV8;
      default:
         break;
   }

   return FRONTEND_ARCH_NONE;
}

static int frontend_uwp_parse_drive_list(void *data, bool load_content)
{
#ifdef HAVE_MENU
   int i;
   char home_dir[DIR_MAX_LENGTH];
   file_list_t            *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content ?
         MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
         MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
   bool have_any_drives         = false;
   DWORD drives                 = GetLogicalDrives();
   home_dir[0]                  = '\0';

   fill_pathname_home_dir(home_dir, sizeof(home_dir));

   for (i = 0; i < 26; i++)
   {
      if (drives & (1 << i))
      {
         TCHAR driveName[] = { TEXT('A') + i, TEXT(':'), TEXT('\\'), TEXT('\0') };
         menu_entries_append(list,
            driveName,
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0, NULL);
         have_any_drives = true;
      }
   }

   menu_entries_append(list,
      home_dir,
      msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
      enum_idx,
      FILE_TYPE_DIRECTORY, 0, 0, NULL);

   if (!have_any_drives)
   {
      menu_entries_append(list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER),
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_BROWSER_OPEN_PICKER),
         MENU_ENUM_LABEL_FILE_BROWSER_OPEN_PICKER,
         MENU_SETTING_ACTION, 0, 0, NULL);

      if (string_is_equal(uwp_device_family, "Windows.Desktop"))
      {
         menu_entries_append(list,
            msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS),
            msg_hash_to_str(
               MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS),
            MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
            MENU_SETTING_ACTION, 0, 0, NULL);
      }
   }
#endif

   return 0;
}

static void frontend_uwp_env_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   /* On UWP, we have to use the writable directory
    * instead of the install directory. */
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_ASSETS],
      "~\\assets\\", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
      "~\\filters\\audio\\", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
      "~\\filters\\video\\", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CHEATS],
      "~\\cheats\\", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_DATABASE],
      "~\\database\\rdb\\", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_PLAYLIST],
      "~\\playlists\\", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG],
      "~\\config\\record\\", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT],
      "~\\recordings\\", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
      "~\\config\\", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_REMAP],
      "~\\config\\remaps\\", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_WALLPAPERS],
      "~\\assets\\wallpapers\\", sizeof(g_defaults.dirs[DEFAULT_DIR_WALLPAPERS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS],
      "~\\thumbnails\\", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_OVERLAY],
      "~\\overlays\\", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY],
      "~\\overlays\\keyboards\\", sizeof(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY]));
   /* This one is an exception: cores have to be loaded from
    * the install directory,
    * since this is the only place UWP apps can take .dlls from */
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CORE],
      ":\\cores\\", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
      "~\\info\\", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG],
      "~\\autoconfig\\", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_SHADER],
      "~\\shaders\\", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS],
      "~\\downloads\\", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT],
      "~\\screenshots\\", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_SRAM],
      "~\\saves\\", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_SAVESTATE],
      "~\\states\\", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_SYSTEM],
      "~\\system\\", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_LOGS],
      "~\\logs\\", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));

#ifdef HAVE_MENU
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES) || defined(HAVE_OPENGL_CORE)
   if (string_is_equal(uwp_device_family, "Windows.Mobile"))
      strlcpy(g_defaults.settings_menu, "glui", sizeof(g_defaults.settings_menu));
#endif
#endif

#ifndef IS_SALAMANDER
   {
      char custom_ini_path[PATH_MAX_LENGTH];
      fill_pathname_expand_special(custom_ini_path,
            "~\\custom.ini", sizeof(custom_ini_path));
      dir_check_defaults(custom_ini_path);
   }
#endif
}

static uint64_t frontend_uwp_get_total_mem(void)
{
   MEMORYSTATUSEX mem_info;
   mem_info.dwLength = sizeof(MEMORYSTATUSEX);
   GlobalMemoryStatusEx(&mem_info);
   return mem_info.ullTotalPhys;
}

static uint64_t frontend_uwp_get_free_mem(void)
{
   MEMORYSTATUSEX mem_info;
   mem_info.dwLength = sizeof(MEMORYSTATUSEX);
   GlobalMemoryStatusEx(&mem_info);
   return (mem_info.ullTotalPhys - mem_info.ullAvailPhys);
}

frontend_ctx_driver_t frontend_ctx_uwp = {
   frontend_uwp_env_get,           /* env_get */
   frontend_uwp_init,              /* init    */
   NULL,                           /* deinit */
   NULL,                           /* exitspawn */
   NULL,                           /* process_args */
   NULL,                           /* exec */
   NULL,                           /* set_fork */
   NULL,                           /* shutdown */
   NULL,                           /* get_name */
   frontend_uwp_get_os,
   NULL,                            /* get_rating */
   NULL,                            /* content_loaded */
   frontend_uwp_get_arch,           /* get_architecture       */
   frontend_uwp_get_powerstate,
   frontend_uwp_parse_drive_list,
   frontend_uwp_get_total_mem,      /* get_total_mem          */
   frontend_uwp_get_free_mem,       /* get_free_mem           */
   NULL,                            /* install_signal_handler */
   NULL,                            /* get_sighandler_state */
   NULL,                            /* set_sighandler_state */
   NULL,                            /* destroy_sighandler_state */
   NULL,                            /* attach_console */
   NULL,                            /* detach_console */
   NULL,                            /* get_lakka_version */
   NULL,                            /* set_screen_brightness */
   NULL,                            /* watch_path_for_changes */
   NULL,                            /* check_for_path_changes */
   NULL,                            /* set_sustained_performance_mode */
   uwp_get_cpu_model_name,          /* get_cpu_model_name  */
   uwp_get_language,                /* get_user_language   */
   NULL,                            /* is_narrator_running */
   NULL,                            /* accessibility_speak */
   NULL,                            /* set_gamemode        */
   "uwp",                           /* ident               */
   NULL                             /* get_video_driver    */
};

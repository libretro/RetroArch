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
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../ui/drivers/ui_win32.h"

#include "../../uwp/uwp_func.h"

static void frontend_uwp_get_os(char *s, size_t len, int *major, int *minor)
{
   char buildStr[11]      = {0};
   bool server            = false;
   const char *arch       = "";

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
   /* Windows 2000 and later */
   SYSTEM_INFO si         = {{0}};
   OSVERSIONINFOEX vi     = {0};
   vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   GetSystemInfo(&si);

   /* Available from NT 3.5 and Win95 */
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
#else
   OSVERSIONINFO vi = {0};
   vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

   /* Available from NT 3.5 and Win95 */
   GetVersionEx(&vi);
#endif

   if (major)
      *major = vi.dwMajorVersion;

   if (minor)
      *minor = vi.dwMinorVersion;

   if (vi.dwMajorVersion == 4 && vi.dwMinorVersion == 0)
      snprintf(buildStr, sizeof(buildStr), "%lu", (DWORD)(LOWORD(vi.dwBuildNumber))); /* Windows 95 build number is in the low-order word only */
   else
      snprintf(buildStr, sizeof(buildStr), "%lu", vi.dwBuildNumber);

   switch (vi.dwMajorVersion)
   {
      case 10:
         if (server)
            strlcpy(s, "Windows Server 2016", len);
         else
            strlcpy(s, "Windows 10", len);
         break;
      case 6:
         switch (vi.dwMinorVersion)
         {
            case 3:
               if (server)
                  strlcpy(s, "Windows Server 2012 R2", len);
               else
                  strlcpy(s, "Windows 8.1", len);
               break;
            case 2:
               if (server)
                  strlcpy(s, "Windows Server 2012", len);
               else
                  strlcpy(s, "Windows 8", len);
               break;
            case 1:
               if (server)
                  strlcpy(s, "Windows Server 2008 R2", len);
               else
                  strlcpy(s, "Windows 7", len);
               break;
            case 0:
               if (server)
                  strlcpy(s, "Windows Server 2008", len);
               else
                  strlcpy(s, "Windows Vista", len);
               break;
            default:
               break;
         }
         break;
      case 5:
         switch (vi.dwMinorVersion)
         {
            case 2:
               if (server)
                  strlcpy(s, "Windows Server 2003", len);
               else
               {
                  /* Yes, XP Pro x64 is a higher version number than XP x86 */
                  if (string_is_equal(arch, "x64"))
                     strlcpy(s, "Windows XP", len);
               }
               break;
            case 1:
               strlcpy(s, "Windows XP", len);
               break;
            case 0:
               strlcpy(s, "Windows 2000", len);
               break;
         }
         break;
      case 4:
         switch (vi.dwMinorVersion)
         {
            case 0:
               if (vi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
                  strlcpy(s, "Windows 95", len);
               else if (vi.dwPlatformId == VER_PLATFORM_WIN32_NT)
                  strlcpy(s, "Windows NT 4.0", len);
               else
                  strlcpy(s, "Unknown", len);
               break;
            case 90:
               strlcpy(s, "Windows ME", len);
               break;
            case 10:
               strlcpy(s, "Windows 98", len);
               break;
         }
         break;
      default:
         snprintf(s, len, "Windows %i.%i", *major, *minor);
         break;
   }

   if (!string_is_empty(arch))
   {
      strlcat(s, " ", len);
      strlcat(s, arch, len);
   }

   strlcat(s, " Build ", len);
   strlcat(s, buildStr, len);

   if (!string_is_empty(vi.szCSDVersion))
   {
      strlcat(s, " ", len);
      strlcat(s, vi.szCSDVersion, len);
   }

   if (!string_is_empty(uwp_device_family))
   {
      strlcat(s, " ", len);
      strlcat(s, uwp_device_family, len);
   }
}

static void frontend_uwp_init(void *data)
{
}

enum frontend_powerstate frontend_uwp_get_powerstate(
      int *seconds, int *percent)
{
   SYSTEM_POWER_STATUS status;
   enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;

   if (!GetSystemPowerStatus(&status))
      return ret;

   if (status.BatteryFlag == 0xFF)
      ret = FRONTEND_POWERSTATE_NONE;
   else if (status.BatteryFlag & (1 << 7))
      ret = FRONTEND_POWERSTATE_NO_SOURCE;
   else if (status.BatteryFlag & (1 << 3))
      ret = FRONTEND_POWERSTATE_CHARGING;
   else if (status.ACLineStatus == 1)
      ret = FRONTEND_POWERSTATE_CHARGED;
   else
      ret = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   *percent  = (int)status.BatteryLifePercent;
   *seconds  = (int)status.BatteryLifeTime;

#ifdef _WIN32
   if (*percent == 255)
      *percent = 0;
#endif

   return ret;
}

enum frontend_architecture frontend_uwp_get_architecture(void)
{
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
   /* Windows 2000 and later */
   SYSTEM_INFO si = {{0}};

   GetSystemInfo(&si);

   switch (si.wProcessorArchitecture)
   {
      case PROCESSOR_ARCHITECTURE_AMD64:
         return FRONTEND_ARCH_X86_64;
         break;
      case PROCESSOR_ARCHITECTURE_INTEL:
         return FRONTEND_ARCH_X86;
         break;
      case PROCESSOR_ARCHITECTURE_ARM:
         return FRONTEND_ARCH_ARM;
         break;
      case PROCESSOR_ARCHITECTURE_ARM64:
         return FRONTEND_ARCH_ARMV8;
         break;
      default:
         break;
   }
#endif

   return FRONTEND_ARCH_NONE;
}

static int frontend_uwp_parse_drive_list(void *data, bool load_content)
{
#ifdef HAVE_MENU
   file_list_t            *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content ?
         MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
         MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
   char drive[]                 = " :\\";
   char *home_dir               = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   bool have_any_drives         = false;

   fill_pathname_home_dir(home_dir, PATH_MAX_LENGTH * sizeof(char));

   for (drive[0] = 'A'; drive[0] <= 'Z'; drive[0]++)
   {
      if (uwp_drive_exists(drive))
      {
         menu_entries_append_enum(list,
            drive,
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
            enum_idx,
            FILE_TYPE_DIRECTORY, 0, 0);
         have_any_drives = true;
      }
   }

   menu_entries_append_enum(list,
      home_dir,
      msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
      enum_idx,
      FILE_TYPE_DIRECTORY, 0, 0);

   if (!have_any_drives)
   {
      menu_entries_append_enum(list,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER),
         msg_hash_to_str(MENU_ENUM_LABEL_FILE_BROWSER_OPEN_PICKER),
         MENU_ENUM_LABEL_FILE_BROWSER_OPEN_PICKER,
         MENU_SETTING_ACTION, 0, 0);

      if (string_is_equal(uwp_device_family, "Windows.Desktop"))
      {
         menu_entries_append_enum(list,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS),
            msg_hash_to_str(MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS),
            MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
            MENU_SETTING_ACTION, 0, 0);
      }
   }

   free(home_dir);
#endif

   return 0;
}

static void frontend_uwp_environment_get(int *argc, char *argv[],
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
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CURSOR],
      "~\\database\\cursors\\", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
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
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT],
      "~\\layouts\\", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
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
   {
      snprintf(g_defaults.settings.menu,
         sizeof(g_defaults.settings.menu), "glui");
   }
   else
   {
      snprintf(g_defaults.settings.menu,
         sizeof(g_defaults.settings.menu), "xmb");
   }
#endif
#endif
}

static uint64_t frontend_uwp_get_mem_total(void)
{
   /* OSes below 2000 don't have the Ex version,
    * and non-Ex cannot work with >4GB RAM */
#if _WIN32_WINNT >= 0x0500
   MEMORYSTATUSEX mem_info;
   mem_info.dwLength = sizeof(MEMORYSTATUSEX);
   GlobalMemoryStatusEx(&mem_info);
   return mem_info.ullTotalPhys;
#else
   MEMORYSTATUS mem_info;
   mem_info.dwLength = sizeof(MEMORYSTATUS);
   GlobalMemoryStatus(&mem_info);
   return mem_info.dwTotalPhys;
#endif
}

static uint64_t frontend_uwp_get_mem_used(void)
{
   /* OSes below 2000 don't have the Ex version,
    * and non-Ex cannot work with >4GB RAM */
#if _WIN32_WINNT >= 0x0500
   MEMORYSTATUSEX mem_info;
   mem_info.dwLength = sizeof(MEMORYSTATUSEX);
   GlobalMemoryStatusEx(&mem_info);
   return ((frontend_uwp_get_mem_total() - mem_info.ullAvailPhys));
#else
   MEMORYSTATUS mem_info;
   mem_info.dwLength = sizeof(MEMORYSTATUS);
   GlobalMemoryStatus(&mem_info);
   return ((frontend_uwp_get_mem_total() - mem_info.dwAvailPhys));
#endif
}

enum retro_language frontend_uwp_get_user_language(void)
{
   return uwp_get_language();
}

static const char* frontend_uwp_get_cpu_model_name(void)
{
   return uwp_get_cpu_model_name();
}

frontend_ctx_driver_t frontend_ctx_uwp = {
   frontend_uwp_environment_get,
   frontend_uwp_init,
   NULL,                           /* deinit */
   NULL,                           /* exitspawn */
   NULL,                           /* process_args */
   NULL,                           /* exec */
   NULL,                           /* set_fork */
   NULL,                           /* shutdown */
   NULL,                           /* get_name */
   frontend_uwp_get_os,
   NULL,                           /* get_rating */
   NULL,                           /* load_content */
   frontend_uwp_get_architecture,
   frontend_uwp_get_powerstate,
   frontend_uwp_parse_drive_list,
   frontend_uwp_get_mem_total,
   frontend_uwp_get_mem_used,
   NULL,                            /* install_signal_handler */
   NULL,                            /* get_sighandler_state */
   NULL,                            /* set_sighandler_state */
   NULL,                            /* destroy_sighandler_state */
   NULL,                            /* attach_console */
   NULL,                            /* detach_console */
   NULL,                            /* watch_path_for_changes */
   NULL,                            /* check_for_path_changes */
   NULL,                            /* set_sustained_performance_mode */
   frontend_uwp_get_cpu_model_name,
   frontend_uwp_get_user_language,
   "uwp"
};

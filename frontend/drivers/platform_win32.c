/* RetroArch - A frontend for libretro.
 * Copyright (C) 2011-2017 - Daniel De Matteis
 * Copyright (C) 2016-2019 - Brad Parker
 * Copyright (C) 2018-2019 - Andrés Suárez
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
#if defined(_WIN32) && !defined(_XBOX)
#include <process.h>
#include <errno.h>
#endif

#include <boolean.h>
#include <compat/strl.h>
#include <dynamic/dylib.h>
#include <lists/file_list.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <features/features_cpu.h>

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
#include "../../paths.h"
#include "platform_win32.h"

#ifndef SM_SERVERR2
#define SM_SERVERR2 89
#endif

/* We only load this library once, so we let it be
 * unloaded at application shutdown, since unloading
 * it early seems to cause issues on some systems.
 */

#ifdef HAVE_DYNAMIC
static dylib_t dwmlib;
static dylib_t shell32lib;
#endif

static char win32_cpu_model_name[64] = {0};

VOID (WINAPI *DragAcceptFiles_func)(HWND, BOOL);

static bool dwm_composition_disabled;

static bool console_needs_free;

#if defined(HAVE_LANGEXTRA) && !defined(_XBOX)
#if (defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500) || !defined(_MSC_VER)
struct win32_lang_pair
{
   unsigned short lang_ident;
   enum retro_language lang;
};

/* https://docs.microsoft.com/en-us/windows/desktop/Intl/language-identifier-constants-and-strings */
const struct win32_lang_pair win32_lang_pairs[] =
{
   /* array order MUST be kept, always largest ID first */
   {0x7c04, RETRO_LANGUAGE_CHINESE_TRADITIONAL}, /* neutral */
   {0x1404, RETRO_LANGUAGE_CHINESE_TRADITIONAL}, /* MO */
   {0x1004, RETRO_LANGUAGE_CHINESE_SIMPLIFIED}, /* SG */
   {0xC04, RETRO_LANGUAGE_CHINESE_TRADITIONAL}, /* HK/PRC */
   {0x816, RETRO_LANGUAGE_PORTUGUESE_PORTUGAL},
   {0x416, RETRO_LANGUAGE_PORTUGUESE_BRAZIL},
   {0x2a, RETRO_LANGUAGE_VIETNAMESE},
   {0x19, RETRO_LANGUAGE_RUSSIAN},
   {0x16, RETRO_LANGUAGE_PORTUGUESE_PORTUGAL},
   {0x15, RETRO_LANGUAGE_POLISH},
   {0x13, RETRO_LANGUAGE_DUTCH},
   {0x12, RETRO_LANGUAGE_KOREAN},
   {0x11, RETRO_LANGUAGE_JAPANESE},
   {0x10, RETRO_LANGUAGE_ITALIAN},
   {0xc, RETRO_LANGUAGE_FRENCH},
   {0xa, RETRO_LANGUAGE_SPANISH},
   {0x9, RETRO_LANGUAGE_ENGLISH},
   {0x8, RETRO_LANGUAGE_GREEK},
   {0x7, RETRO_LANGUAGE_GERMAN},
   {0x4, RETRO_LANGUAGE_CHINESE_SIMPLIFIED}, /* neutral */
   {0x1, RETRO_LANGUAGE_ARABIC},
   /* MS does not support Esperanto */
   /*{0x0, RETRO_LANGUAGE_ESPERANTO},*/
};

unsigned short win32_get_langid_from_retro_lang(enum retro_language lang)
{
   unsigned i;

   for (i = 0; i < sizeof(win32_lang_pairs) / sizeof(win32_lang_pairs[0]); i++)
   {
      if (win32_lang_pairs[i].lang == lang)
         return win32_lang_pairs[i].lang_ident;
   }

   return 0x409; /* fallback to US English */
}

enum retro_language win32_get_retro_lang_from_langid(unsigned short langid)
{
   unsigned i;

   for (i = 0; i < sizeof(win32_lang_pairs) / sizeof(win32_lang_pairs[0]); i++)
   {
      if (win32_lang_pairs[i].lang_ident > 0x3ff)
      {
         if (langid == win32_lang_pairs[i].lang_ident)
            return win32_lang_pairs[i].lang;
      }
      else
      {
         if ((langid & 0x3ff) == win32_lang_pairs[i].lang_ident)
            return win32_lang_pairs[i].lang;
      }
   }

   return RETRO_LANGUAGE_ENGLISH;
}
#endif
#endif

static void gfx_dwm_shutdown(void)
{
#ifdef HAVE_DYNAMIC
   if (dwmlib)
      dylib_close(dwmlib);
   if (shell32lib)
      dylib_close(shell32lib);
   dwmlib     = NULL;
   shell32lib = NULL;
#endif
}

static bool gfx_init_dwm(void)
{
   HRESULT (WINAPI *mmcss)(BOOL);
   static bool inited = false;

   if (inited)
      return true;

   atexit(gfx_dwm_shutdown);

#ifdef HAVE_DYNAMIC
   shell32lib = dylib_load("shell32.dll");
   if (!shell32lib)
   {
      RARCH_WARN("Did not find shell32.dll.\n");
   }

   dwmlib = dylib_load("dwmapi.dll");
   if (!dwmlib)
   {
      RARCH_WARN("Did not find dwmapi.dll.\n");
      return false;
   }

   DragAcceptFiles_func =
      (VOID (WINAPI*)(HWND, BOOL))dylib_proc(shell32lib, "DragAcceptFiles");

   mmcss =
      (HRESULT(WINAPI*)(BOOL))dylib_proc(dwmlib, "DwmEnableMMCSS");
#else
   DragAcceptFiles_func = DragAcceptFiles;
#if 0
   mmcss                = DwmEnableMMCSS;
#endif
#endif

   if (mmcss)
   {
      RARCH_LOG("Setting multimedia scheduling for DWM.\n");
      mmcss(TRUE);
   }

   inited = true;
   return true;
}

static void gfx_set_dwm(void)
{
   HRESULT ret;
   HRESULT (WINAPI *composition_enable)(UINT);
   settings_t *settings = config_get_ptr();

   if (!gfx_init_dwm())
      return;

   if (settings->bools.video_disable_composition == dwm_composition_disabled)
      return;

#ifdef HAVE_DYNAMIC
   composition_enable =
      (HRESULT (WINAPI*)(UINT))dylib_proc(dwmlib, "DwmEnableComposition");
#endif

   if (!composition_enable)
   {
      RARCH_ERR("Did not find DwmEnableComposition ...\n");
      return;
   }

   ret = composition_enable(!settings->bools.video_disable_composition);
   if (FAILED(ret))
      RARCH_ERR("Failed to set composition state ...\n");
   dwm_composition_disabled = settings->bools.video_disable_composition;
}

static void frontend_win32_get_os(char *s, size_t len, int *major, int *minor)
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
               {
                  strlcpy(s, "Windows Server 2003", len);
                  if (GetSystemMetrics(SM_SERVERR2))
                     strlcat(s, " R2", len);
               }
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

}

static void frontend_win32_init(void *data)
{
   typedef BOOL (WINAPI *isProcessDPIAwareProc)();
   typedef BOOL (WINAPI *setProcessDPIAwareProc)();
#ifdef HAVE_DYNAMIC
   HMODULE handle                         =
      GetModuleHandle("User32.dll");
   isProcessDPIAwareProc  isDPIAwareProc  =
      (isProcessDPIAwareProc)dylib_proc(handle, "IsProcessDPIAware");
   setProcessDPIAwareProc setDPIAwareProc =
      (setProcessDPIAwareProc)dylib_proc(handle, "SetProcessDPIAware");
#else
   isProcessDPIAwareProc  isDPIAwareProc  = IsProcessDPIAware;
   setProcessDPIAwareProc setDPIAwareProc = SetProcessDPIAware;
#endif

   if (isDPIAwareProc)
      if (!isDPIAwareProc())
         if (setDPIAwareProc)
            setDPIAwareProc();
}

enum frontend_powerstate frontend_win32_get_powerstate(int *seconds, int *percent)
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

enum frontend_architecture frontend_win32_get_architecture(void)
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
      default:
         break;
   }
#endif

   return FRONTEND_ARCH_NONE;
}

static int frontend_win32_parse_drive_list(void *data, bool load_content)
{
#ifdef HAVE_MENU
   file_list_t *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content ?
         MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
         MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
   size_t i          = 0;
   unsigned drives   = GetLogicalDrives();
   char    drive[]   = " :\\";

   for (i = 0; i < 32; i++)
   {
      drive[0] = 'A' + i;
      if (drives & (1 << i))
         menu_entries_append_enum(list,
               drive,
               msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
               enum_idx,
               FILE_TYPE_DIRECTORY, 0, 0);
   }
#endif

   return 0;
}

static void frontend_win32_environment_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   gfx_set_dwm();

   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_ASSETS],
      ":\\assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
      ":\\filters\\audio", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
      ":\\filters\\video", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CHEATS],
      ":\\cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_DATABASE],
      ":\\database\\rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CURSOR],
      ":\\database\\cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_PLAYLIST],
      ":\\playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG],
      ":\\config\\record", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT],
      ":\\recordings", sizeof(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
      ":\\config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_REMAP],
      ":\\config\\remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_WALLPAPERS],
      ":\\assets\\wallpapers", sizeof(g_defaults.dirs[DEFAULT_DIR_WALLPAPERS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS],
      ":\\thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_OVERLAY],
      ":\\overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
#ifdef HAVE_VIDEO_LAYOUT
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT],
      ":\\layouts", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]));
#endif
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CORE],
      ":\\cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
      ":\\info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG],
      ":\\autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_SHADER],
      ":\\shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS],
      ":\\downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT],
      ":\\screenshots", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_SRAM],
      ":\\saves", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_SAVESTATE],
      ":\\states", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_SYSTEM],
      ":\\system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_LOGS],
      ":\\logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
#ifdef HAVE_MENU
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGLES) || defined(HAVE_OPENGL_CORE)
   strlcpy(g_defaults.settings.menu, "xmb", sizeof(g_defaults.settings.menu));
#endif
#endif
}

static uint64_t frontend_win32_get_mem_total(void)
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

static uint64_t frontend_win32_get_mem_used(void)
{
   /* OSes below 2000 don't have the Ex version,
    * and non-Ex cannot work with >4GB RAM */
#if _WIN32_WINNT >= 0x0500
   MEMORYSTATUSEX mem_info;
   mem_info.dwLength = sizeof(MEMORYSTATUSEX);
   GlobalMemoryStatusEx(&mem_info);
   return ((frontend_win32_get_mem_total() - mem_info.ullAvailPhys));
#else
   MEMORYSTATUS mem_info;
   mem_info.dwLength = sizeof(MEMORYSTATUS);
   GlobalMemoryStatus(&mem_info);
   return ((frontend_win32_get_mem_total() - mem_info.dwAvailPhys));
#endif
}

static void frontend_win32_attach_console(void)
{
#ifdef _WIN32
#ifdef _WIN32_WINNT_WINXP
   /* msys will start the process with FILE_TYPE_PIPE connected.
    *   cmd will start the process with FILE_TYPE_UNKNOWN connected
    *   (since this is subsystem windows application
    * ... UNLESS stdout/stderr were redirected (then FILE_TYPE_DISK
    * will be connected most likely)
    * explorer will start the process with NOTHING connected.
    *
    * Now, let's not reconnect anything that's already connected.
    * If any are disconnected, open a console, and connect to them.
    * In case we're launched from msys or cmd, try attaching to the
    * parent process console first.
    *
    * Take care to leave a record of what we did, so we can
    * undo it precisely.
    */

   bool need_stdout = (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE))
         == FILE_TYPE_UNKNOWN);
   bool need_stderr = (GetFileType(GetStdHandle(STD_ERROR_HANDLE))
         == FILE_TYPE_UNKNOWN);

   if(need_stdout || need_stderr)
   {
      if(!AttachConsole( ATTACH_PARENT_PROCESS))
         AllocConsole();

      SetConsoleTitle("Log Console");

      if(need_stdout) freopen( "CONOUT$", "w", stdout );
      if(need_stderr) freopen( "CONOUT$", "w", stderr );

      console_needs_free = true;
   }

#endif
#endif
}

static void frontend_win32_detach_console(void)
{
#if defined(_WIN32) && !defined(_XBOX)
#ifdef _WIN32_WINNT_WINXP
   if(console_needs_free)
   {
      /* we don't reconnect stdout/stderr to anything here,
       * because by definition, they weren't connected to
       * anything in the first place. */
      FreeConsole();
      console_needs_free = false;
   }
#endif
#endif
}

static const char* frontend_win32_get_cpu_model_name(void)
{
#ifdef ANDROID
   return NULL;
#else
   cpu_features_get_model_name(win32_cpu_model_name, sizeof(win32_cpu_model_name));
   return win32_cpu_model_name;
#endif
}

enum retro_language frontend_win32_get_user_language(void)
{
   enum retro_language lang = RETRO_LANGUAGE_ENGLISH;
#if defined(HAVE_LANGEXTRA) && !defined(_XBOX)
#if (defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500) || !defined(_MSC_VER)
   LANGID langid = GetUserDefaultUILanguage();
   lang = win32_get_retro_lang_from_langid(langid);
#endif
#endif
   return lang;
}

#if defined(_WIN32) && !defined(_XBOX)
enum frontend_fork win32_fork_mode;

static void frontend_win32_respawn(char *s, size_t len)
{
   STARTUPINFO si;
   PROCESS_INFORMATION pi;
   char executable_path[PATH_MAX_LENGTH] = {0};

   if (win32_fork_mode != FRONTEND_FORK_RESTART)
      return;

   memset(&si, 0, sizeof(si));
   si.cb = sizeof(si);
   memset(&pi, 0, sizeof(pi));

   fill_pathname_application_path(executable_path,
         sizeof(executable_path));
   path_set(RARCH_PATH_CORE, executable_path);
   RARCH_LOG("Restarting RetroArch with commandline: %s and %s\n",
      executable_path, get_retroarch_launch_arguments());

   if(!CreateProcess( executable_path, get_retroarch_launch_arguments(),
      NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
   {
      RARCH_LOG("Failed to restart RetroArch\n");
   }
   return;
}

static bool frontend_win32_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         break;
      case FRONTEND_FORK_RESTART:
         command_event(CMD_EVENT_QUIT, NULL);
         break;
      case FRONTEND_FORK_NONE:
      default:
         break;
   }
   win32_fork_mode = fork_mode;
   return true;
}
#endif

frontend_ctx_driver_t frontend_ctx_win32 = {
   frontend_win32_environment_get,
   frontend_win32_init,
   NULL,                           /* deinit */
#if defined(_WIN32) && !defined(_XBOX)
   frontend_win32_respawn,         /* exitspawn */
#else
   NULL,                           /* exitspawn */
#endif
   NULL,                           /* process_args */
   NULL,                           /* exec */
#if defined(_WIN32) && !defined(_XBOX)
   frontend_win32_set_fork,        /* set_fork */
#else
   NULL,                           /* set_fork */
#endif
   NULL,                           /* shutdown */
   NULL,                           /* get_name */
   frontend_win32_get_os,
   NULL,                           /* get_rating */
   NULL,                           /* load_content */
   frontend_win32_get_architecture,
   frontend_win32_get_powerstate,
   frontend_win32_parse_drive_list,
   frontend_win32_get_mem_total,
   frontend_win32_get_mem_used,
   NULL,                            /* install_signal_handler */
   NULL,                            /* get_sighandler_state */
   NULL,                            /* set_sighandler_state */
   NULL,                            /* destroy_sighandler_state */
   frontend_win32_attach_console,   /* attach_console */
   frontend_win32_detach_console,   /* detach_console */
   NULL,                            /* watch_path_for_changes */
   NULL,                            /* check_for_path_changes */
   NULL,                            /* set_sustained_performance_mode */
   frontend_win32_get_cpu_model_name,
   frontend_win32_get_user_language,
   "win32"
};

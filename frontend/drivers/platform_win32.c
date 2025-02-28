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
#endif

#include <boolean.h>
#include <compat/strl.h>
#include <dynamic/dylib.h>
#include <lists/file_list.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
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
#include "../../paths.h"
#include "../../msg_hash.h"
#include "../../verbosity.h"
#include "../../ui/drivers/ui_win32.h"

#include "platform_win32.h"

#ifdef HAVE_SAPI
#define COBJMACROS
#include <sapi.h>
#include <ole2.h>
#endif

#ifndef SM_SERVERR2
#define SM_SERVERR2 89
#endif

enum platform_win32_flags
{
   PLAT_WIN32_FLAG_USE_POWERSHELL           = (1 << 0),
   PLAT_WIN32_FLAG_USE_NVDA                 = (1 << 1),
   PLAT_WIN32_FLAG_USE_NVDA_BRAILLE         = (1 << 2),
   PLAT_WIN32_FLAG_DWM_COMPOSITION_DISABLED = (1 << 3),
   PLAT_WIN32_FLAG_CONSOLE_NEEDS_FREE       = (1 << 4),
   PLAT_WIN32_FLAG_PROCESS_INSTANCE_SET     = (1 << 5)
};

#ifdef HAVE_SAPI
static ISpVoice *voice_ptr        = NULL;
#endif
#ifdef HAVE_NVDA
static uint8_t g_plat_win32_flags = PLAT_WIN32_FLAG_USE_NVDA;
#else
static uint8_t g_plat_win32_flags = PLAT_WIN32_FLAG_USE_POWERSHELL;
#endif

/* static public global variable */
VOID (WINAPI *DragAcceptFiles_func)(HWND, BOOL);

/* TODO/FIXME - static global variables */
static char win32_cpu_model_name[64] = {0};
#ifdef HAVE_DYLIB
/* We only load this library once, so we let it be
 * unloaded at application shutdown, since unloading
 * it early seems to cause issues on some systems.
 */
static dylib_t dwm_lib;
static dylib_t shell32_lib;
static dylib_t nvda_lib;
#endif

/* Dynamic loading for Non-Visual Desktop Access support */
unsigned long (__stdcall *nvdaController_testIfRunning_func)(void);
unsigned long (__stdcall *nvdaController_cancelSpeech_func)(void);
unsigned long (__stdcall *nvdaController_brailleMessage_func)(wchar_t*);
unsigned long (__stdcall *nvdaController_speakText_func)(wchar_t*);

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
   {0x1004, RETRO_LANGUAGE_CHINESE_SIMPLIFIED},  /* SG */
   {0xC04,  RETRO_LANGUAGE_CHINESE_TRADITIONAL}, /* HK/PRC */
   {0x816,  RETRO_LANGUAGE_PORTUGUESE_PORTUGAL},
   {0x416,  RETRO_LANGUAGE_PORTUGUESE_BRAZIL},
   {0x2a,   RETRO_LANGUAGE_VIETNAMESE},
   {0x19,   RETRO_LANGUAGE_RUSSIAN},
   {0x16,   RETRO_LANGUAGE_PORTUGUESE_PORTUGAL},
   {0x15,   RETRO_LANGUAGE_POLISH},
   {0x13,   RETRO_LANGUAGE_DUTCH},
   {0x12,   RETRO_LANGUAGE_KOREAN},
   {0x11,   RETRO_LANGUAGE_JAPANESE},
   {0x10,   RETRO_LANGUAGE_ITALIAN},
   {0xc,    RETRO_LANGUAGE_FRENCH},
   {0xa,    RETRO_LANGUAGE_SPANISH},
   {0x9,    RETRO_LANGUAGE_ENGLISH},
   {0x8,    RETRO_LANGUAGE_GREEK},
   {0x7,    RETRO_LANGUAGE_GERMAN},
   {0x4,    RETRO_LANGUAGE_CHINESE_SIMPLIFIED},  /* neutral */
   {0x1,    RETRO_LANGUAGE_ARABIC},
   /* MS does not support Esperanto */
   /*{0x0, RETRO_LANGUAGE_ESPERANTO},*/
};

unsigned short win32_get_langid_from_retro_lang(enum retro_language lang)
{
   size_t i;

   for (i = 0; i < ARRAY_SIZE(win32_lang_pairs); i++)
   {
      if (win32_lang_pairs[i].lang == lang)
         return win32_lang_pairs[i].lang_ident;
   }

   return 0x409; /* fallback to US English */
}

enum retro_language win32_get_retro_lang_from_langid(unsigned short langid)
{
   size_t i;

   for (i = 0; i < ARRAY_SIZE(win32_lang_pairs); i++)
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
#else
unsigned short win32_get_langid_from_retro_lang(enum retro_language lang)
{
   return 0x409; /* fallback to US English */
}

enum retro_language win32_get_retro_lang_from_langid(unsigned short langid)
{
   return RETRO_LANGUAGE_ENGLISH;
}
#endif

static void gfx_dwm_shutdown(void)
{
#ifdef HAVE_DYLIB
   if (dwm_lib)
      dylib_close(dwm_lib);
   if (shell32_lib)
      dylib_close(shell32_lib);
   dwm_lib     = NULL;
   shell32_lib = NULL;
#endif
}

static bool gfx_init_dwm(void)
{
   HRESULT (WINAPI *mmcss)(BOOL);
   static bool inited = false;

   if (inited)
      return true;

   atexit(gfx_dwm_shutdown);

#ifdef HAVE_DYLIB
   if (!(shell32_lib = dylib_load("shell32.dll")))
   {
      RARCH_WARN("Did not find shell32.dll.\n");
   }

   if (!(dwm_lib = dylib_load("dwmapi.dll")))
   {
      RARCH_WARN("Did not find dwmapi.dll.\n");
      return false;
   }

   DragAcceptFiles_func =
      (VOID (WINAPI*)(HWND, BOOL))dylib_proc(shell32_lib, "DragAcceptFiles");

   mmcss =
      (HRESULT(WINAPI*)(BOOL))dylib_proc(dwm_lib, "DwmEnableMMCSS");
#else
   DragAcceptFiles_func = DragAcceptFiles;
#endif

   if (mmcss)
      mmcss(TRUE);

   inited = true;
   return true;
}

static void gfx_set_dwm(void)
{
   HRESULT ret;
   HRESULT (WINAPI *composition_enable)(UINT);
   settings_t *settings     = config_get_ptr();
   bool disable_composition = settings->bools.video_disable_composition;

   if (!gfx_init_dwm())
      return;

   if (disable_composition == (g_plat_win32_flags &
            PLAT_WIN32_FLAG_DWM_COMPOSITION_DISABLED))
      return;

#ifdef HAVE_DYLIB
   composition_enable =
      (HRESULT (WINAPI*)(UINT))dylib_proc(dwm_lib, "DwmEnableComposition");
#endif

   if (!composition_enable)
   {
      RARCH_ERR("Did not find DwmEnableComposition ...\n");
      return;
   }

   ret = composition_enable(!disable_composition);
   if (FAILED(ret))
      RARCH_ERR("Failed to set composition state ...\n");
   if (disable_composition)
      g_plat_win32_flags |= PLAT_WIN32_FLAG_DWM_COMPOSITION_DISABLED;
}

static size_t frontend_win32_get_os(char *s, size_t len, int *major, int *minor)
{
   size_t _len            = 0;
   char build_str[11]     = {0};
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
      snprintf(build_str, sizeof(build_str), "%lu", (DWORD)(LOWORD(vi.dwBuildNumber))); /* Windows 95 build number is in the low-order word only */
   else
      snprintf(build_str, sizeof(build_str), "%lu", vi.dwBuildNumber);

   switch (vi.dwMajorVersion)
   {
      case 10:
         if (atoi(build_str) >= 21996)
            _len = strlcpy(s, "Windows 11", len);
         else if (server)
            _len = strlcpy(s, "Windows Server 2016", len);
         else
            _len = strlcpy(s, "Windows 10", len);
         break;
      case 6:
         switch (vi.dwMinorVersion)
         {
            case 3:
               if (server)
                  _len = strlcpy(s, "Windows Server 2012 R2", len);
               else
                  _len = strlcpy(s, "Windows 8.1", len);
               break;
            case 2:
               if (server)
                  _len = strlcpy(s, "Windows Server 2012", len);
               else
                  _len = strlcpy(s, "Windows 8", len);
               break;
            case 1:
               if (server)
                  _len = strlcpy(s, "Windows Server 2008 R2", len);
               else
                  _len = strlcpy(s, "Windows 7", len);
               break;
            case 0:
               if (server)
                  _len = strlcpy(s, "Windows Server 2008", len);
               else
                  _len = strlcpy(s, "Windows Vista", len);
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
                  _len = strlcpy(s, "Windows Server 2003", len);
                  if (GetSystemMetrics(SM_SERVERR2))
                     _len += strlcpy(s + _len, " R2", len - _len);
               }
               else
               {
                  /* Yes, XP Pro x64 is a higher version number than XP x86 */
                  if (string_is_equal(arch, "x64"))
                     _len = strlcpy(s, "Windows XP", len);
               }
               break;
            case 1:
               _len = strlcpy(s, "Windows XP", len);
               break;
            case 0:
               _len = strlcpy(s, "Windows 2000", len);
               break;
         }
         break;
      case 4:
         switch (vi.dwMinorVersion)
         {
            case 0:
               if (vi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
                  _len = strlcpy(s, "Windows 95", len);
               else if (vi.dwPlatformId == VER_PLATFORM_WIN32_NT)
                  _len = strlcpy(s, "Windows NT 4.0", len);
               else
                  _len = strlcpy(s, "Unknown", len);
               break;
            case 90:
               _len = strlcpy(s, "Windows ME", len);
               break;
            case 10:
               _len = strlcpy(s, "Windows 98", len);
               break;
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
      strlcpy(s + _len, vi.szCSDVersion, len - _len);
   }
   return _len;
}

static void frontend_win32_init(void *data)
{
   typedef BOOL (WINAPI *isProcessDPIAwareProc)();
   typedef BOOL (WINAPI *setProcessDPIAwareProc)();
#ifdef HAVE_DYLIB
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


#ifdef HAVE_NVDA
static void init_nvda(void)
{
#ifdef HAVE_DYLIB
   if (     (g_plat_win32_flags & PLAT_WIN32_FLAG_USE_NVDA)
         && !nvda_lib)
   {
      if ((nvda_lib = dylib_load("nvdaControllerClient64.dll")))
      {
         nvdaController_testIfRunning_func  = (unsigned long (__stdcall*)(void))dylib_proc(nvda_lib, "nvdaController_testIfRunning");
         nvdaController_cancelSpeech_func   = (unsigned long(__stdcall *)(void))dylib_proc(nvda_lib, "nvdaController_cancelSpeech");
         nvdaController_brailleMessage_func = (unsigned long(__stdcall *)(wchar_t*))dylib_proc(nvda_lib, "nvdaController_brailleMessage");
         nvdaController_speakText_func      = (unsigned long(__stdcall *)(wchar_t*))dylib_proc(nvda_lib, "nvdaController_speakText");
         return;
      }
   }
#endif
   /* The above code is executed on each accessibility speak event, so
    * we should only revert to powershell if nvda_lib wasn't loaded previously,
    * and we weren't able to load it on this call, or we don't HAVE_DYLIB */
   if ((g_plat_win32_flags & PLAT_WIN32_FLAG_USE_NVDA) && !nvda_lib)
   {
      g_plat_win32_flags &= ~PLAT_WIN32_FLAG_USE_NVDA;
      g_plat_win32_flags |=  PLAT_WIN32_FLAG_USE_POWERSHELL;
   }
}
#endif

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

enum frontend_architecture frontend_win32_get_arch(void)
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
         menu_entries_append(list,
               drive,
               msg_hash_to_str(MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
               enum_idx,
               FILE_TYPE_DIRECTORY, 0, 0, NULL);
   }
#endif

   return 0;
}

static void frontend_win32_env_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   const char *tmp_dir = getenv("TMP");
   const char *libretro_directory = getenv("LIBRETRO_DIRECTORY");
   const char *libretro_assets_directory = getenv("LIBRETRO_ASSETS_DIRECTORY");
   const char* libretro_autoconfig_directory = getenv("LIBRETRO_AUTOCONFIG_DIRECTORY");
   const char* libretro_cheats_directory = getenv("LIBRETRO_CHEATS_DIRECTORY");
   const char* libretro_database_directory = getenv("LIBRETRO_DATABASE_DIRECTORY");
   const char* libretro_system_directory = getenv("LIBRETRO_SYSTEM_DIRECTORY");
   const char* libretro_video_filter_directory = getenv("LIBRETRO_VIDEO_FILTER_DIRECTORY");
   const char* libretro_video_shader_directory = getenv("LIBRETRO_VIDEO_SHADER_DIRECTORY");
   if (!string_is_empty(tmp_dir))
      fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CACHE],
         tmp_dir, sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));

   gfx_set_dwm();

   if (!string_is_empty(libretro_assets_directory))
      strlcpy(g_defaults.dirs[DEFAULT_DIR_ASSETS], libretro_assets_directory,
	      sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   else
       fill_pathname_expand_special(
	   g_defaults.dirs[DEFAULT_DIR_ASSETS],
	   ":\\assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER],
      ":\\filters\\audio", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   if (!string_is_empty(libretro_video_filter_directory))
       strlcpy(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
	       libretro_video_filter_directory,
	       sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   else
       fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER],
           ":\\filters\\video", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   if (!string_is_empty(libretro_cheats_directory))
       strlcpy(g_defaults.dirs[DEFAULT_DIR_CHEATS],
	       libretro_cheats_directory,
	       sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   else
       fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CHEATS],
           ":\\cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   if (!string_is_empty(libretro_database_directory))
       strlcpy(g_defaults.dirs[DEFAULT_DIR_DATABASE],
	       libretro_database_directory,
	       sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   else
       fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_DATABASE],
           ":\\database\\rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_PLAYLIST],
      ":\\playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
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
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY],
      ":\\overlays\\keyboards", sizeof(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY]));
   if (!string_is_empty(libretro_directory))
      strlcpy(g_defaults.dirs[DEFAULT_DIR_CORE], libretro_directory,
            sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   else
      fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CORE],
            ":\\cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   if (!string_is_empty(libretro_directory))
      strlcpy(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], libretro_directory,
            sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   else
       fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
           ":\\info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   if (!string_is_empty(libretro_autoconfig_directory))
      strlcpy(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG],
	      libretro_autoconfig_directory,
	      sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   else
       fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG],
             ":\\autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   if (!string_is_empty(libretro_video_filter_directory))
      strlcpy(g_defaults.dirs[DEFAULT_DIR_SHADER],
	      libretro_video_shader_directory,
	      sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
   else
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
   if (!string_is_empty(libretro_system_directory))
       strlcpy(g_defaults.dirs[DEFAULT_DIR_SYSTEM],
	       libretro_system_directory,
	       sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   else
       fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_SYSTEM],
             ":\\system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_expand_special(g_defaults.dirs[DEFAULT_DIR_LOGS],
      ":\\logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
#endif
}

static uint64_t frontend_win32_get_total_mem(void)
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

static uint64_t frontend_win32_get_free_mem(void)
{
   /* OSes below 2000 don't have the Ex version,
    * and non-Ex cannot work with >4GB RAM */
#if _WIN32_WINNT >= 0x0500
   MEMORYSTATUSEX mem_info;
   mem_info.dwLength = sizeof(MEMORYSTATUSEX);
   GlobalMemoryStatusEx(&mem_info);
   return mem_info.ullAvailPhys;
#else
   MEMORYSTATUS mem_info;
   mem_info.dwLength = sizeof(MEMORYSTATUS);
   GlobalMemoryStatus(&mem_info);
   return mem_info.dwAvailPhys;
#endif
}

static void frontend_win32_attach_console(void)
{
#ifdef _WIN32
#ifdef _WIN32_WINNT_WINXP
   /* MSys will start the process with FILE_TYPE_PIPE connected.
    * cmd will start the process with FILE_TYPE_UNKNOWN connected
    *   (since this is subsystem windows application
    * ... UNLESS stdout/stderr were redirected (then FILE_TYPE_DISK
    * will be connected most likely)
    * Explorer will start the process with NOTHING connected.
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

   if (config_get_ptr()->bools.log_to_file)
      return;

   if (need_stdout || need_stderr)
   {
      if (!AttachConsole(ATTACH_PARENT_PROCESS))
         AllocConsole();

      SetConsoleTitle("Log Console");

      if (need_stdout)
         freopen("CONOUT$", "w", stdout);
      if (need_stderr)
         freopen("CONOUT$", "w", stderr);

      g_plat_win32_flags |= PLAT_WIN32_FLAG_CONSOLE_NEEDS_FREE;
   }
#endif
#endif
}

static void frontend_win32_detach_console(void)
{
#if defined(_WIN32) && !defined(_XBOX)
#ifdef _WIN32_WINNT_WINXP
   if (g_plat_win32_flags & PLAT_WIN32_FLAG_CONSOLE_NEEDS_FREE)
   {
      /* We don't reconnect stdout/stderr to anything here,
       * because by definition, they weren't connected to
       * anything in the first place. */
      FreeConsole();
      g_plat_win32_flags &= ~PLAT_WIN32_FLAG_CONSOLE_NEEDS_FREE;
   }
#endif
#endif
}

static const char* frontend_win32_get_cpu_model_name(void)
{
   cpu_features_get_model_name(win32_cpu_model_name, sizeof(win32_cpu_model_name));
   return win32_cpu_model_name;
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

static void frontend_win32_respawn(char *s, size_t len, char *args)
{
   STARTUPINFO si;
   PROCESS_INFORMATION pi;
   char executable_path[PATH_MAX_LENGTH] = {0};

   if (win32_fork_mode != FRONTEND_FORK_RESTART)
      return;

   GetModuleFileName(NULL, executable_path, PATH_MAX_LENGTH);
   path_set(RARCH_PATH_CORE, executable_path);

   memset(&si, 0, sizeof(si));
   si.cb = sizeof(si);
   memset(&pi, 0, sizeof(pi));

   if (!CreateProcess(executable_path, GetCommandLine(),
         NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
      RARCH_ERR("Failed to restart RetroArch\n");
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

#if defined(_WIN32) && !defined(_XBOX)
static const char *accessibility_win_language_id(const char* language)
{
   if (string_is_equal(language,"en"))
      return "409";
   else if (string_is_equal(language,"it"))
      return "410";
   else if (string_is_equal(language,"sv"))
      return "041d";
   else if (string_is_equal(language,"fr"))
      return "040c";
   else if (string_is_equal(language,"de"))
      return "407";
   else if (string_is_equal(language,"he"))
      return "040d";
   else if (string_is_equal(language,"id"))
      return "421";
   else if (string_is_equal(language,"es"))
      return "040a";
   else if (string_is_equal(language,"nl"))
      return "413";
   else if (string_is_equal(language,"ro"))
      return "418";
   else if (string_is_equal(language,"pt_pt"))
      return "816";
   else if (string_is_equal(language,"pt_bt") || string_is_equal(language,"pt"))
      return "416";
   else if (string_is_equal(language,"th"))
      return "041e";
   else if (string_is_equal(language,"ja"))
      return "411";
   else if (string_is_equal(language,"sk"))
      return "041b";
   else if (string_is_equal(language,"hi"))
      return "439";
   else if (string_is_equal(language,"ar"))
      return "401";
   else if (string_is_equal(language,"hu"))
      return "040e";
   else if (string_is_equal(language, "zh_tw") || string_is_equal(language,"zh"))
      return "804";
   else if (string_is_equal(language,"el"))
      return "408";
   else if (string_is_equal(language,"ru"))
      return "419";
   else if (string_is_equal(language,"nb"))
      return "414";
   else if (string_is_equal(language,"da"))
      return "406";
   else if (string_is_equal(language,"fi"))
      return "040b";
   else if (string_is_equal(language,"zh_hk"))
      return "0c04";
   else if (string_is_equal(language,"zh_cn"))
      return "804";
   else if (string_is_equal(language,"tr"))
      return "041f";
   else if (string_is_equal(language,"ko"))
      return "412";
   else if (string_is_equal(language,"pl"))
      return "415";
   else if (string_is_equal(language,"cs"))
      return "405";
   return "";
}

static const char *accessibility_win_language_code(const char* language)
{
   if (string_is_equal(language,"en"))
      return "Microsoft David Desktop";
   else if (string_is_equal(language,"it"))
      return "Microsoft Cosimo Desktop";
   else if (string_is_equal(language,"sv"))
      return "Microsoft Bengt Desktop";
   else if (string_is_equal(language,"fr"))
      return "Microsoft Paul Desktop";
   else if (string_is_equal(language,"de"))
      return "Microsoft Stefan Desktop";
   else if (string_is_equal(language,"he"))
      return "Microsoft Asaf Desktop";
   else if (string_is_equal(language,"id"))
      return "Microsoft Andika Desktop";
   else if (string_is_equal(language,"es"))
      return "Microsoft Pablo Desktop";
   else if (string_is_equal(language,"nl"))
      return "Microsoft Frank Desktop";
   else if (string_is_equal(language,"ro"))
      return "Microsoft Andrei Desktop";
   else if (string_is_equal(language,"pt_pt"))
      return "Microsoft Helia Desktop";
   else if (string_is_equal(language,"pt_bt") || string_is_equal(language,"pt"))
      return "Microsoft Daniel Desktop";
   else if (string_is_equal(language,"th"))
      return "Microsoft Pattara Desktop";
   else if (string_is_equal(language,"ja"))
      return "Microsoft Ichiro Desktop";
   else if (string_is_equal(language,"sk"))
      return "Microsoft Filip Desktop";
   else if (string_is_equal(language,"hi"))
      return "Microsoft Hemant Desktop";
   else if (string_is_equal(language,"ar"))
      return "Microsoft Naayf Desktop";
   else if (string_is_equal(language,"hu"))
      return "Microsoft Szabolcs Desktop";
   else if (string_is_equal(language, "zh_tw")
            || string_is_equal(language,"zh-TW")
            || string_is_equal(language,"zh"))
      return "Microsoft Zhiwei Desktop";
   else if (string_is_equal(language,"el"))
      return "Microsoft Stefanos Desktop";
   else if (string_is_equal(language,"ru"))
      return "Microsoft Pavel Desktop";
   else if (string_is_equal(language,"no") || string_is_equal(language,"nb"))
      return "Microsoft Jon Desktop";
   else if (string_is_equal(language,"da"))
      return "Microsoft Helle Desktop";
   else if (string_is_equal(language,"fi"))
      return "Microsoft Heidi Desktop";
   else if (string_is_equal(language,"zh_hk"))
      return "Microsoft Danny Desktop";
   else if (string_is_equal(language,"zh_cn") || string_is_equal(language,"zh-CN"))
      return "Microsoft Kangkang Desktop";
   else if (string_is_equal(language,"tr"))
      return "Microsoft Tolga Desktop";
   else if (string_is_equal(language,"ko"))
      return "Microsoft Heami Desktop";
   else if (string_is_equal(language,"pl"))
      return "Microsoft Adam Desktop";
   else if (string_is_equal(language,"cs"))
      return "Microsoft Jakub Desktop";
   else if (string_is_equal(language,"vi"))
      return "Microsoft An Desktop";
   else if (string_is_equal(language,"hr"))
      return "Microsoft Matej Desktop";
   else if (string_is_equal(language,"bg"))
      return "Microsoft Ivan Desktop";
   else if (string_is_equal(language,"ms"))
      return "Microsoft Rizwan Desktop";
   else if (string_is_equal(language,"sl"))
      return "Microsoft Lado Desktop";
   else if (string_is_equal(language,"ta"))
      return "Microsoft Valluvar Desktop";
   else if (string_is_equal(language,"en_gb"))
      return "Microsoft George Desktop";
   else if (string_is_equal(language,"ca") || string_is_equal(language,"ca_ES@valencia"))
      return "Microsoft Herena Desktop";
   return "";
}

static void terminate_win32_process(PROCESS_INFORMATION pi)
{
   TerminateProcess(pi.hProcess,0);
   CloseHandle(pi.hProcess);
   CloseHandle(pi.hThread);
}

static PROCESS_INFORMATION g_pi;

static bool create_win32_process(char* cmd, const char * input)
{
   STARTUPINFO si;
   HANDLE rd = NULL;
   bool ret  = false;
   memset(&si, 0, sizeof(si));
   si.cb = sizeof(si);
   memset(&g_pi, 0, sizeof(g_pi));

   if (input)
   {
      DWORD dummy;
      HANDLE wr;
      size_t input_len = strlen(input);
      if (!CreatePipe(&rd, &wr, NULL, input_len))
         return false;

      SetHandleInformation(rd, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

      WriteFile(wr, input, input_len, &dummy, NULL);
      CloseHandle(wr);

      si.dwFlags    |= STARTF_USESTDHANDLES;
      si.hStdInput   = rd;
      si.hStdOutput  = GetStdHandle(STD_OUTPUT_HANDLE);
      si.hStdError   = GetStdHandle(STD_ERROR_HANDLE);
   }

   ret = CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                      NULL, NULL, &si, &g_pi);
   if (rd)
      CloseHandle(rd);
   return ret;
}

static bool is_narrator_running_windows(void)
{
   DWORD status = 0;
#ifdef HAVE_NVDA
   init_nvda();
#endif

   if (g_plat_win32_flags & PLAT_WIN32_FLAG_USE_POWERSHELL)
   {
      if (!(g_plat_win32_flags & PLAT_WIN32_FLAG_PROCESS_INSTANCE_SET))
         return false;
      if (GetExitCodeProcess(g_pi.hProcess, &status))
         if (status == STILL_ACTIVE)
            return true;
      return false;
   }
#ifdef HAVE_NVDA
   else if (g_plat_win32_flags & PLAT_WIN32_FLAG_USE_NVDA)
   {
      long res = nvdaController_testIfRunning_func();

      if (res != 0)
      {
         /* The running nvda service wasn't found, so revert
            back to the powershell method
         */
         RARCH_ERR("Error communicating with NVDA\n");
         g_plat_win32_flags |=  PLAT_WIN32_FLAG_USE_POWERSHELL;
         g_plat_win32_flags &= ~PLAT_WIN32_FLAG_USE_NVDA;
         return false;
      }
      return false;
   }
#endif
#ifdef HAVE_SAPI
   else
   {
      if (voice_ptr)
      {
         SPVOICESTATUS status_ptr;
         ISpVoice_GetStatus(voice_ptr, &status_ptr, NULL);
         if (status_ptr.dwRunningState == SPRS_IS_SPEAKING)
            return true;
      }
   }
#endif
   return false;
}

static bool accessibility_speak_windows(int speed,
      const char* speak_text, int priority)
{
   char cmd[512];
   const char *voice      = get_user_language_iso639_1(true);
   const char *language   = accessibility_win_language_code(voice);
   const char *langid     = accessibility_win_language_id(voice);
   bool res               = false;
   const char* speeds[10] = {"-10", "-7.5", "-5", "-2.5", "0", "2", "4", "6", "8", "10"};
   size_t nbytes_cmd      = 0;
   if (speed < 1)
      speed               = 1;
   else if (speed > 10)
      speed               = 10;

   if (priority < 10)
   {
      if (is_narrator_running_windows())
         return true;
   }
#ifdef HAVE_NVDA
   init_nvda();
#endif

   if (g_plat_win32_flags & PLAT_WIN32_FLAG_USE_POWERSHELL)
   {
      const char * template_lang = "powershell.exe -NoProfile -WindowStyle Hidden -Command \"Add-Type -AssemblyName System.Speech; $synth = New-Object System.Speech.Synthesis.SpeechSynthesizer; $synth.SelectVoice(\\\"%s\\\"); $synth.Rate = %s; $synth.Speak($input);\"";
      const char * template_nolang = "powershell.exe -NoProfile -WindowStyle Hidden -Command \"Add-Type -AssemblyName System.Speech; $synth = New-Object System.Speech.Synthesis.SpeechSynthesizer; $synth.Rate = %s; $synth.Speak($input);\"";
      if (language && language[0] != '\0')
         snprintf(cmd, sizeof(cmd), template_lang, language, speeds[speed-1]);
      else
         snprintf(cmd, sizeof(cmd), template_nolang, speeds[speed-1]);
      if (g_plat_win32_flags & PLAT_WIN32_FLAG_PROCESS_INSTANCE_SET)
         terminate_win32_process(g_pi);
      if (create_win32_process(cmd, speak_text))
         g_plat_win32_flags |=  PLAT_WIN32_FLAG_PROCESS_INSTANCE_SET;
      else
         g_plat_win32_flags &= ~PLAT_WIN32_FLAG_PROCESS_INSTANCE_SET;
   }
#ifdef HAVE_NVDA
   else if (g_plat_win32_flags & PLAT_WIN32_FLAG_USE_NVDA)
   {
      wchar_t        *wc = utf8_to_utf16_string_alloc(speak_text);
      long res           = nvdaController_testIfRunning_func();

      if (!wc || res != 0)
      {
         RARCH_ERR("Error communicating with NVDA\n");
         /* Fallback on powershell immediately and retry */
         g_plat_win32_flags &= ~PLAT_WIN32_FLAG_USE_NVDA;
         g_plat_win32_flags |= PLAT_WIN32_FLAG_USE_POWERSHELL;
         if (wc)
            free(wc);
         return accessibility_speak_windows(speed, speak_text, priority);
      }

      nvdaController_cancelSpeech_func();

      if (g_plat_win32_flags & PLAT_WIN32_FLAG_USE_NVDA_BRAILLE)
         nvdaController_brailleMessage_func(wc);
      else
         nvdaController_speakText_func(wc);
      free(wc);
   }
#endif
#ifdef HAVE_SAPI
   else
   {
      HRESULT hr;
      /* stop the old voice if running */
      if (voice_ptr)
      {
         CoUninitialize();
         ISpVoice_Release(voice_ptr);
      }
      voice_ptr = NULL;

      /* Play the new voice */
      if (FAILED(CoInitialize(NULL)))
         return NULL;

      hr = CoCreateInstance(&CLSID_SpVoice, NULL,
            CLSCTX_ALL, &IID_ISpVoice, (void **)&voice_ptr);

      if (SUCCEEDED(hr))
      {
         wchar_t *wc = utf8_to_utf16_string_alloc(speak_text);
         if (!wc)
            return false;
         hr = ISpVoice_Speak(voice_ptr, wc, SPF_ASYNC /*SVSFlagsAsync*/, NULL);
         free(wc);
      }
   }
#endif

   return true;
}
#endif

frontend_ctx_driver_t frontend_ctx_win32 = {
   frontend_win32_env_get,         /* env_get   */
   frontend_win32_init,            /* init      */
   NULL,                           /* deinit    */
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
   NULL,                           /* shutdown                  */
   NULL,                           /* get_name                  */
   frontend_win32_get_os,
   NULL,                           /* get_rating                */
   NULL,                           /* content_loaded            */
   frontend_win32_get_arch,        /* get_architecture          */
   frontend_win32_get_powerstate,
   frontend_win32_parse_drive_list,
   frontend_win32_get_total_mem,
   frontend_win32_get_free_mem,
   NULL,                            /* install_signal_handler   */
   NULL,                            /* get_sighandler_state     */
   NULL,                            /* set_sighandler_state     */
   NULL,                            /* destroy_sighandler_state */
   frontend_win32_attach_console,   /* attach_console           */
   frontend_win32_detach_console,   /* detach_console           */
   NULL,                            /* get_lakka_version        */
   NULL,                            /* set_screen_brightness    */
   NULL,                            /* watch_path_for_changes   */
   NULL,                            /* check_for_path_changes   */
   NULL,                            /* set_sustained_performance_mode */
   frontend_win32_get_cpu_model_name,
   frontend_win32_get_user_language,
#if defined(_WIN32) && !defined(_XBOX)
   is_narrator_running_windows,     /* is_narrator_running */
   accessibility_speak_windows,     /* accessibility_speak */
#else
   NULL,                            /* is_narrator_running */
   NULL,                            /* accessibility_speak */
#endif
   NULL,                            /* set_gamemode        */
   "win32",                         /* ident               */
   NULL                             /* get_video_driver    */
};

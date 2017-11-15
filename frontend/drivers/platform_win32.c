/* RetroArch - A frontend for libretro.
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

#include <retro_miscellaneous.h>
#include <windows.h>

#include <boolean.h>
#include <compat/strl.h>
#include <dynamic/dylib.h>
#include <lists/file_list.h>
#include <file/file_path.h>

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

/* We only load this library once, so we let it be 
 * unloaded at application shutdown, since unloading 
 * it early seems to cause issues on some systems.
 */

static dylib_t dwmlib;
static dylib_t shell32lib;

VOID (WINAPI *DragAcceptFiles_func)(HWND, BOOL);

static bool dwm_composition_disabled;

static bool console_needs_free;

static void gfx_dwm_shutdown(void)
{
   if (dwmlib)
      dylib_close(dwmlib);
   if (shell32lib)
      dylib_close(shell32lib);
   dwmlib     = NULL;
   shell32lib = NULL;
}

static bool gfx_init_dwm(void)
{
   HRESULT (WINAPI *mmcss)(BOOL);
   static bool inited = false;

   if (inited)
      return true;

   atexit(gfx_dwm_shutdown);

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
      (HRESULT (WINAPI*)(BOOL))dylib_proc(dwmlib, "DwmEnableMMCSS");
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

   composition_enable = 
      (HRESULT (WINAPI*)(UINT))dylib_proc(dwmlib, "DwmEnableComposition");
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
	uint32_t version = GetVersion();

	*major   = (DWORD)(LOBYTE(LOWORD(version)));
	*minor   = (DWORD)(HIBYTE(LOWORD(version)));

   switch (*major)
   {
      case 10:
         strlcpy(s, "Windows 10", len);
         break;
      case 6:
         switch (*minor)
         {
            case 3:
               strlcpy(s, "Windows 8.1", len);
               break;
            case 2:
               strlcpy(s, "Windows 8", len);
               break;
            case 1:
               strlcpy(s, "Windows 7/2008 R2", len);
               break;
            case 0:
               strlcpy(s, "Windows Vista/2008", len);
               break;
            default:
               break;
         }
         break;
      case 5:
         switch (*minor)
         {
            case 2:
               strlcpy(s, "Windows 2003", len);
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
         switch (*minor)
         {
            case 0:
               strlcpy(s, "Windows NT 4.0", len);
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
         sprintf(s, "Windows %i.%i", *major, *minor);
         break;
   }
}

static void frontend_win32_init(void *data)
{
	typedef BOOL (WINAPI *isProcessDPIAwareProc)();
	typedef BOOL (WINAPI *setProcessDPIAwareProc)();
	HMODULE handle                         = 
      GetModuleHandle("User32.dll");
	isProcessDPIAwareProc  isDPIAwareProc  = 
      (isProcessDPIAwareProc)dylib_proc(handle, "IsProcessDPIAware");
	setProcessDPIAwareProc setDPIAwareProc = 
      (setProcessDPIAwareProc)dylib_proc(handle, "SetProcessDPIAware");

	if (isDPIAwareProc)
	{
		if (!isDPIAwareProc())
		{
			if (setDPIAwareProc)
				setDPIAwareProc();
		}
	}
   
}

enum frontend_powerstate frontend_win32_get_powerstate(int *seconds, int *percent)
{
   SYSTEM_POWER_STATUS status;
	enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;

	if (!GetSystemPowerStatus(&status))
		return ret;

	if (status.BatteryFlag == 0xFF)
		ret = FRONTEND_POWERSTATE_NONE;
	if (status.BatteryFlag & (1 << 7))
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
   /* stub */
   return FRONTEND_ARCH_NONE;
}

static int frontend_win32_parse_drive_list(void *data, bool load_content)
{
#ifdef HAVE_MENU
   size_t i          = 0;
   unsigned drives   = GetLogicalDrives();
   char    drive[]   = " :\\";
   file_list_t *list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content ?
      MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR :
      MSG_UNKNOWN;

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

#ifdef HAVE_MENU
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   snprintf(g_defaults.settings.menu,
         sizeof(g_defaults.settings.menu), "xmb");
#endif
#endif
}

static uint64_t frontend_win32_get_mem_total(void)
{
   /* OSes below 2000 don't have the Ex version,
    * and non-Ex cannot work with >4GB RAM */
#if _WIN32_WINNT > 0x0400
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
#if _WIN32_WINNT > 0x0400
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

frontend_ctx_driver_t frontend_ctx_win32 = {
   frontend_win32_environment_get,
   frontend_win32_init,
   NULL,                           /* deinit */
   NULL,                           /* exitspawn */
   NULL,                           /* process_args */
   NULL,                           /* exec */
   NULL,                           /* set_fork */
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
   "win32"
};

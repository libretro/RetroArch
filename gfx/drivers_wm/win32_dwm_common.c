/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "win32_dwm_common.h"
#include "../../general.h"

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#include "../../dynamic.h"

/* We only load this library once, so we let it be 
 * unloaded at application shutdown, since unloading 
 * it early seems to cause issues on some systems.
 */

static dylib_t dwmlib;
static bool dwm_composition_disabled;

static void gfx_dwm_shutdown(void)
{
   if (dwmlib)
      dylib_close(dwmlib);
   dwmlib = NULL;
}

static bool gfx_init_dwm(void)
{
   static bool inited = false;

   if (inited)
      return true;

   dwmlib = dylib_load("dwmapi.dll");
   if (!dwmlib)
   {
      RARCH_LOG("Did not find dwmapi.dll.\n");
      return false;
   }
   atexit(gfx_dwm_shutdown);

   HRESULT (WINAPI *mmcss)(BOOL) = 
      (HRESULT (WINAPI*)(BOOL))dylib_proc(dwmlib, "DwmEnableMMCSS");
   if (mmcss)
   {
      RARCH_LOG("Setting multimedia scheduling for DWM.\n");
      mmcss(TRUE);
   }

   inited = true;
   return true;
}

void gfx_set_dwm(void)
{
   HRESULT ret;
   settings_t *settings = config_get_ptr();

   if (!gfx_init_dwm())
      return;

   if (settings->video.disable_composition == dwm_composition_disabled)
      return;

   HRESULT (WINAPI *composition_enable)(UINT) = 
      (HRESULT (WINAPI*)(UINT))dylib_proc(dwmlib, "DwmEnableComposition");
   if (!composition_enable)
   {
      RARCH_ERR("Did not find DwmEnableComposition ...\n");
      return;
   }

   ret = composition_enable(!settings->video.disable_composition);
   if (FAILED(ret))
      RARCH_ERR("Failed to set composition state ...\n");
   dwm_composition_disabled = settings->video.disable_composition;
}
#endif

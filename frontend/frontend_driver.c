/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdio.h>
#include <string.h>

#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>
#include <libretro.h>

#if defined(_3DS)
#include <3ds.h>
#endif

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "frontend_driver.h"

#ifndef __WINRT__
#if defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#define __WINRT__
#endif
#endif

static frontend_ctx_driver_t frontend_ctx_null = {
   NULL,                         /* environment_get */
   NULL,                         /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   NULL,                         /* get_rating */
   NULL,                         /* load_content */
   NULL,                         /* get_architecture */
   NULL,                         /* get_powerstate */
   NULL,                         /* parse_drive_list */
   NULL,                         /* get_mem_total */
   NULL,                         /* get_mem_free */
   NULL,                         /* install_signal_handler */
   NULL,                         /* get_sighandler_state */
   NULL,                         /* set_sighandler_state */
   NULL,                         /* destroy_sighandler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
#ifdef HAVE_LAKKA
   NULL,                         /* get_lakka_version */
#endif
   NULL,                         /* watch_path_for_changes */
   NULL,                         /* check_for_path_changes */
   NULL,                         /* set_sustained_performance_mode */
   NULL,                         /* get_cpu_model_name */
   NULL,                         /* get_user_language */
   NULL,                         /* is_narrator_running */
   NULL,                         /* accessibility_speak */
   "null",
   NULL,                         /* get_video_driver */
};

static frontend_ctx_driver_t *frontend_ctx_drivers[] = {
#if defined(EMSCRIPTEN)
   &frontend_ctx_emscripten,
#elif defined(__CELLOS_LV2__)
   &frontend_ctx_ps3,
#endif
#if defined(_XBOX)
   &frontend_ctx_xdk,
#endif
#if defined(GEKKO)
   &frontend_ctx_gx,
#endif
#if defined(WIIU)
   &frontend_ctx_wiiu,
#endif
#if defined(__QNX__)
   &frontend_ctx_qnx,
#endif
#if defined(__APPLE__) && defined(__MACH__)
   &frontend_ctx_darwin,
#endif
#if defined(__linux__) || (defined(BSD) && !defined(__MACH__))
   &frontend_ctx_unix,
#endif
#if defined(PSP) || defined(VITA)
   &frontend_ctx_psp,
#endif
#if defined(PS2)
   &frontend_ctx_ps2,
#endif
#if defined(_3DS)
   &frontend_ctx_ctr,
#endif
#if defined(SWITCH) && defined(HAVE_LIBNX)
   &frontend_ctx_switch,
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   &frontend_ctx_win32,
#endif
#if defined(__WINRT__)
   &frontend_ctx_uwp,
#endif
#ifdef XENON
   &frontend_ctx_xenon,
#endif
#ifdef DJGPP
   &frontend_ctx_dos,
#endif
#ifdef SWITCH
   &frontend_ctx_switch,
#endif
#if defined(ORBIS)
   &frontend_ctx_orbis,
#endif
   &frontend_ctx_null,
   NULL
};

/**
 * frontend_ctx_find_driver:
 * @ident               : Identifier name of driver to find.
 *
 * Finds driver with @ident. Does not initialize.
 *
 * Returns: pointer to driver if successful, otherwise NULL.
 **/
frontend_ctx_driver_t *frontend_ctx_find_driver(const char *ident)
{
   unsigned i;

   for (i = 0; frontend_ctx_drivers[i]; i++)
   {
      if (string_is_equal(frontend_ctx_drivers[i]->ident, ident))
         return frontend_ctx_drivers[i];
   }

   return NULL;
}

/**
 * frontend_ctx_init_first:
 *
 * Finds first suitable driver and initialize.
 *
 * Returns: pointer to first suitable driver, otherwise NULL.
 **/
frontend_ctx_driver_t *frontend_ctx_init_first(void)
{
   return frontend_ctx_drivers[0];
}

bool frontend_driver_get_core_extension(char *s, size_t len)
{
#ifdef HAVE_DYNAMIC

#ifdef _WIN32
   strlcpy(s, "dll", len);
   return true;
#elif defined(__APPLE__) || defined(__MACH__)
   strlcpy(s, "dylib", len);
   return true;
#else
   strlcpy(s, "so", len);
   return true;
#endif

#else

#if defined(__CELLOS_LV2__)
   strlcpy(s, "self|bin", len);
   return true;
#elif defined(PSP)
   strlcpy(s, "pbp", len);
   return true;
#elif defined(VITA)
   strlcpy(s, "self|bin", len);
   return true;
#elif defined(PS2)
   strlcpy(s, "elf", len);
   return true;
#elif defined(_XBOX1)
   strlcpy(s, "xbe", len);
   return true;
#elif defined(_XBOX360)
   strlcpy(s, "xex", len);
   return true;
#elif defined(GEKKO)
   strlcpy(s, "dol", len);
   return true;
#elif defined(HW_WUP)
   strlcpy(s, "rpx|elf", len);
   return true;
#elif defined(__linux__)
   strlcpy(s, "elf", len);
   return true;
#elif defined(HAVE_LIBNX)
   strlcpy(s, "nro", len);
   return true;
#elif defined(DJGPP)
   strlcpy(s, "exe", len);
   return true;
#elif defined(_3DS)
   if (envIsHomebrew())
      strlcpy(s, "3dsx", len);
   else
      strlcpy(s, "cia", len);
   return true;
#else
   return false;
#endif

#endif
}

bool frontend_driver_get_salamander_basename(char *s, size_t len)
{
#ifdef HAVE_DYNAMIC
   return false;
#else

#if defined(__CELLOS_LV2__)
   strlcpy(s, "EBOOT.BIN", len);
   return true;
#elif defined(PSP)
   strlcpy(s, "EBOOT.PBP", len);
   return true;
#elif defined(VITA)
   strlcpy(s, "eboot.bin", len);
   return true;
#elif defined(PS2)
   strlcpy(s, "eboot.elf", len);
   return true;
#elif defined(_XBOX1)
   strlcpy(s, "default.xbe", len);
   return true;
#elif defined(_XBOX360)
   strlcpy(s, "default.xex", len);
   return true;
#elif defined(HW_RVL)
   strlcpy(s, "boot.dol", len);
   return true;
#elif defined(HW_WUP)
   strlcpy(s, "retroarch.rpx", len);
   return true;
#elif defined(_3DS)
   strlcpy(s, "retroarch.core", len);
   return true;
#elif defined(DJGPP)
   strlcpy(s, "retrodos.exe", len);
   return true;
#elif defined(SWITCH)
   strlcpy(s, "retroarch_switch.nro", len);
   return true;
#else
   return false;
#endif

#endif
}

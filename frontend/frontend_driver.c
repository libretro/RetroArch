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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(_3DS)
#include <3ds.h>
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
   NULL,                         /* get_lakka_version */
   NULL,                         /* set_screen_brightness */
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
#endif
#if defined(__PS3__)
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
   strcpy_literal(s, "dll");
   return true;
#elif defined(__APPLE__) || defined(__MACH__)
   strcpy_literal(s, "dylib");
   return true;
#else
   strcpy_literal(s, "so");
   return true;
#endif

#else

#if defined(PSP)
   strcpy_literal(s, "pbp");
   return true;
#elif defined(VITA)
   strcpy_literal(s, "self|bin");
   return true;
#elif defined(PS2)
   strcpy_literal(s, "elf");
   return true;
#elif defined(__PS3__)
   strcpy_literal(s, "self|bin");
   return true;
#elif defined(_XBOX1)
   strcpy_literal(s, "xbe");
   return true;
#elif defined(_XBOX360)
   strcpy_literal(s, "xex");
   return true;
#elif defined(GEKKO)
   strcpy_literal(s, "dol");
   return true;
#elif defined(HW_WUP)
   strcpy_literal(s, "rpx|elf");
   return true;
#elif defined(__linux__)
   strcpy_literal(s, "elf");
   return true;
#elif defined(HAVE_LIBNX)
   strcpy_literal(s, "nro");
   return true;
#elif defined(DJGPP)
   strcpy_literal(s, "exe");
   return true;
#elif defined(_3DS)
   if (envIsHomebrew())
      strcpy_literal(s, "3dsx");
   else
      strcpy_literal(s, "cia");
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

#if defined(PSP)
   strcpy_literal(s, "EBOOT.PBP");
   return true;
#elif defined(VITA)
   strcpy_literal(s, "eboot.bin");
   return true;
#elif defined(PS2)
   strcpy_literal(s, "eboot.elf");
   return true;
#elif defined(__PSL1GHT__) || defined(__PS3__)
   strcpy_literal(s, "EBOOT.BIN");
   return true;
#elif defined(_XBOX1)
   strcpy_literal(s, "default.xbe");
   return true;
#elif defined(_XBOX360)
   strcpy_literal(s, "default.xex");
   return true;
#elif defined(HW_RVL)
   strcpy_literal(s, "boot.dol");
   return true;
#elif defined(HW_WUP)
   strcpy_literal(s, "retroarch.rpx");
   return true;
#elif defined(_3DS)
   strcpy_literal(s, "retroarch.core");
   return true;
#elif defined(DJGPP)
   strcpy_literal(s, "retrodos.exe");
   return true;
#elif defined(SWITCH)
   strcpy_literal(s, "retroarch_switch.nro");
   return true;
#else
   return false;
#endif

#endif
}

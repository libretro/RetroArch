/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <string.h>

#include <compat/strl.h>

#include "frontend_driver.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static frontend_ctx_driver_t *frontend_ctx_drivers[] = {
#if defined(__CELLOS_LV2__)
   &frontend_ctx_ps3,
#endif
#if defined(_XBOX)
   &frontend_ctx_xdk,
#endif
#if defined(GEKKO)
   &frontend_ctx_gx,
#endif
#if defined(__QNX__)
   &frontend_ctx_qnx,
#endif
#if defined(__APPLE__) && defined(__MACH__)
   &frontend_ctx_darwin,
#endif
#if defined(__linux__)
   &frontend_ctx_linux,
#endif
#if defined(PSP) || defined(VITA)
   &frontend_ctx_psp,
#endif
#if defined(_3DS)
   &frontend_ctx_ctr,
#endif
#if defined(_WIN32) && !defined(_XBOX)
   &frontend_ctx_win32,
#endif
#ifdef XENON
   &frontend_ctx_xenon,
#endif
   &frontend_ctx_null,
   NULL
};

#ifndef IS_SALAMANDER
static frontend_ctx_driver_t *current_frontend_ctx;
#endif

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
      if (!strcmp(frontend_ctx_drivers[i]->ident, ident))
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
   unsigned i;
   frontend_ctx_driver_t *frontend = NULL;

   for (i = 0; frontend_ctx_drivers[i]; i++)
   {
      frontend = frontend_ctx_drivers[i];
      break;
   }

   return frontend;
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
   strlcpy(s, "velf", len);
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
#elif defined(__linux__)
   strlcpy(s, "elf", len);
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
   strlcpy(s, "default.velf", len);
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
#else
   return false;
#endif

#endif
}

#ifndef IS_SALAMANDER
frontend_ctx_driver_t *frontend_get_ptr(void)
{
   return current_frontend_ctx;
}

int frontend_driver_parse_drive_list(void *data)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();

   if (!frontend || !frontend->parse_drive_list)
      return -1;
   return frontend->parse_drive_list(data);
}

void frontend_driver_content_loaded(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();

   if (!frontend || !frontend->content_loaded)
      return;
   frontend->content_loaded();
}

bool frontend_driver_has_fork(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();

   if (!frontend || !frontend->set_fork)
      return false;
   return true;
}

bool frontend_driver_set_fork(enum frontend_fork fork_mode)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();

   if (!frontend_driver_has_fork())
      return false;
   return frontend->set_fork(fork_mode);
}

void frontend_driver_process_args(int *argc, char *argv[])
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();

   if (!frontend || !frontend->process_args)
      return;
   frontend->process_args(argc, argv);
}

bool frontend_driver_is_inited(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend)
      return false;
   return true;
}

void frontend_driver_init_first(void *args)
{
   current_frontend_ctx = (frontend_ctx_driver_t*)frontend_ctx_init_first();

   if (current_frontend_ctx && current_frontend_ctx->init)
      current_frontend_ctx->init(args);
}

void frontend_driver_free(void)
{
   current_frontend_ctx = NULL;
}

environment_get_t frontend_driver_environment_get_ptr(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend)
      return NULL;
   return frontend->environment_get;
}

bool frontend_driver_has_get_video_driver_func(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->get_video_driver)
      return false;
   return true;
}

const struct video_driver *frontend_driver_get_video_driver(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->get_video_driver)
      return NULL;
   return frontend->get_video_driver();
}

void frontend_driver_exitspawn(char *s, size_t len)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->exitspawn)
      return;
   frontend->exitspawn(s, len);
}

void frontend_driver_deinit(void *args)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->deinit)
      return;
   frontend->deinit(args);
}

void frontend_driver_shutdown(bool a)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->shutdown)
      return;
   frontend->shutdown(a);
}
#endif

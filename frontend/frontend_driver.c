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

#include <string.h>

#include <compat/strl.h>
#include <string/stdstring.h>
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
#elif defined(SWITCH)
   strlcpy(s, "retroarch_switch.nro", len);
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

int frontend_driver_parse_drive_list(void *data, bool load_content)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();

   if (!frontend || !frontend->parse_drive_list)
      return -1;
   return frontend->parse_drive_list(data, load_content);
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

enum frontend_architecture frontend_driver_get_cpu_architecture(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->get_architecture)
      return FRONTEND_ARCH_NONE;
   return frontend->get_architecture();
}

uint64_t frontend_driver_get_total_memory(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->get_total_mem)
      return 0;
   return frontend->get_total_mem();
}

uint64_t frontend_driver_get_free_memory(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->get_free_mem)
      return 0;
   return frontend->get_free_mem();
}

void frontend_driver_install_signal_handler(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->install_signal_handler)
      return;
   frontend->install_signal_handler();
}

int frontend_driver_get_signal_handler_state(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->get_signal_handler_state)
      return -1;
   return frontend->get_signal_handler_state();
}

void frontend_driver_set_signal_handler_state(int value)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->set_signal_handler_state)
      return;
   frontend->set_signal_handler_state(value);
}

void frontend_driver_attach_console(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->attach_console)
      return;
   frontend->attach_console();
}

void frontend_driver_detach_console(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->detach_console)
      return;
   frontend->detach_console();
}

void frontend_driver_destroy_signal_handler_state(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->destroy_signal_handler_state)
      return;
   frontend->destroy_signal_handler_state();
}

bool frontend_driver_can_watch_for_changes(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->watch_path_for_changes)
      return false;
   return true;
}

void frontend_driver_watch_path_for_changes(struct string_list *list, int flags, path_change_data_t **change_data)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->watch_path_for_changes)
      return;
   frontend->watch_path_for_changes(list, flags, change_data);
}

bool frontend_driver_check_for_path_changes(path_change_data_t *change_data)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->check_for_path_changes)
      return false;
   return frontend->check_for_path_changes(change_data);
}

void frontend_driver_set_sustained_performance_mode(bool on)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->set_sustained_performance_mode)
      return;
   frontend->set_sustained_performance_mode(on);
}

const char* frontend_driver_get_cpu_model_name(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->get_cpu_model_name)
      return NULL;
   return frontend->get_cpu_model_name();
}

enum retro_language frontend_driver_get_user_language(void)
{
   frontend_ctx_driver_t *frontend = frontend_get_ptr();
   if (!frontend || !frontend->get_user_language)
      return RETRO_LANGUAGE_ENGLISH;
   return frontend->get_user_language();
}
#endif

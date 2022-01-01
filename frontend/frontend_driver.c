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
   NULL,                         /* set_gamemode */
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

static frontend_state_t frontend_driver_st = { 0 };

frontend_state_t *frontend_state_get_ptr(void)
{
   return &frontend_driver_st;
}

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

frontend_ctx_driver_t *frontend_get_ptr(void)
{
   frontend_state_t *frontend_st = &frontend_driver_st;
   return frontend_st->current_frontend_ctx;
}

int frontend_driver_parse_drive_list(void *data, bool load_content)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->parse_drive_list)
      return frontend->parse_drive_list(data, load_content);
   return -1;
}

void frontend_driver_content_loaded(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->content_loaded)
      frontend->content_loaded();
}

bool frontend_driver_has_fork(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   return frontend && frontend->set_fork;
}

bool frontend_driver_set_fork(enum frontend_fork fork_mode)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (!frontend || !frontend_driver_has_fork())
      return false;
   return frontend->set_fork(fork_mode);
}

void frontend_driver_process_args(int *argc, char *argv[])
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->process_args)
      frontend->process_args(argc, argv);
}

bool frontend_driver_is_inited(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   return frontend != NULL;
}

void frontend_driver_init_first(void *args)
{
   frontend_state_t *frontend_st     = &frontend_driver_st;
   frontend_st->current_frontend_ctx = (frontend_ctx_driver_t*)
      frontend_ctx_init_first();

   if (     frontend_st->current_frontend_ctx 
         && frontend_st->current_frontend_ctx->init)
      frontend_st->current_frontend_ctx->init(args);
}

void frontend_driver_free(void)
{
   frontend_state_t *frontend_st     = &frontend_driver_st;

   frontend_st->current_frontend_ctx = NULL;
}

bool frontend_driver_has_get_video_driver_func(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   return frontend && frontend->get_video_driver;
}

const struct video_driver *frontend_driver_get_video_driver(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (!frontend || !frontend->get_video_driver)
      return NULL;
   return frontend->get_video_driver();
}

void frontend_driver_exitspawn(char *s, size_t len, char *args)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->exitspawn)
      frontend->exitspawn(s, len, args);
}

void frontend_driver_deinit(void *args)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->deinit)
      frontend->deinit(args);
}

void frontend_driver_shutdown(bool a)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->shutdown)
      frontend->shutdown(a);
}

enum frontend_architecture frontend_driver_get_cpu_architecture(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->get_architecture)
      return frontend->get_architecture();
   return FRONTEND_ARCH_NONE;
}

const void *frontend_driver_get_cpu_architecture_str(
      char *architecture, size_t size)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   enum frontend_architecture arch = frontend_driver_get_cpu_architecture();

   switch (arch)
   {
      case FRONTEND_ARCH_X86:
         strcpy_literal(architecture, "x86");
         break;
      case FRONTEND_ARCH_X86_64:
         strcpy_literal(architecture, "x64");
         break;
      case FRONTEND_ARCH_PPC:
         strcpy_literal(architecture, "PPC");
         break;
      case FRONTEND_ARCH_ARM:
         strcpy_literal(architecture, "ARM");
         break;
      case FRONTEND_ARCH_ARMV7:
         strcpy_literal(architecture, "ARMv7");
         break;
      case FRONTEND_ARCH_ARMV8:
         strcpy_literal(architecture, "ARMv8");
         break;
      case FRONTEND_ARCH_MIPS:
         strcpy_literal(architecture, "MIPS");
         break;
      case FRONTEND_ARCH_TILE:
         strcpy_literal(architecture, "Tilera");
         break;
      case FRONTEND_ARCH_NONE:
      default:
         strcpy_literal(architecture, "N/A");
         break;
   }

   return frontend;
}

uint64_t frontend_driver_get_total_memory(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->get_total_mem)
      return frontend->get_total_mem();
   return 0;
}

uint64_t frontend_driver_get_free_memory(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->get_free_mem)
      return frontend->get_free_mem();
   return 0;
}

void frontend_driver_install_signal_handler(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->install_signal_handler)
      frontend->install_signal_handler();
}

int frontend_driver_get_signal_handler_state(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->get_signal_handler_state)
      return frontend->get_signal_handler_state();
   return -1;
}

void frontend_driver_set_signal_handler_state(int value)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->set_signal_handler_state)
      frontend->set_signal_handler_state(value);
}

void frontend_driver_attach_console(void)
{
   /* TODO/FIXME - the frontend driver code is garbage and needs to be
      redesigned. Apparently frontend_driver_attach_console can be called
      BEFORE frontend_driver_init_first is called, hence why we need 
      to resort to the check for non-NULL below. This is just awful, 
      BEFORE we make any frontend function call, we should be 100% 
      sure frontend_driver_init_first has already been called first.

      For now, we do this hack, but this absolutely should be redesigned
      as soon as possible.
    */
   if(      frontend_driver_st.current_frontend_ctx 
         && frontend_driver_st.current_frontend_ctx->attach_console)
      frontend_driver_st.current_frontend_ctx->attach_console();
}

void frontend_driver_set_screen_brightness(int value)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->set_screen_brightness)
      frontend->set_screen_brightness(value);
}

bool frontend_driver_can_set_screen_brightness(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   return (frontend && frontend->set_screen_brightness);
}

void frontend_driver_detach_console(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->detach_console)
      frontend->detach_console();
}

void frontend_driver_destroy_signal_handler_state(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->destroy_signal_handler_state)
      frontend->destroy_signal_handler_state();
}

bool frontend_driver_can_watch_for_changes(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   return frontend && frontend->watch_path_for_changes;
}

void frontend_driver_watch_path_for_changes(
      struct string_list *list, int flags,
      path_change_data_t **change_data)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->watch_path_for_changes)
      frontend->watch_path_for_changes(list, flags, change_data);
}

bool frontend_driver_check_for_path_changes(path_change_data_t *change_data)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->check_for_path_changes)
      return frontend->check_for_path_changes(change_data);
   return false;
}

void frontend_driver_set_sustained_performance_mode(bool on)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->set_sustained_performance_mode)
      frontend->set_sustained_performance_mode(on);
}

const char* frontend_driver_get_cpu_model_name(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->get_cpu_model_name)
      return frontend->get_cpu_model_name();
   return NULL;
}

enum retro_language frontend_driver_get_user_language(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->get_user_language)
      return frontend->get_user_language();
   return RETRO_LANGUAGE_ENGLISH;
}

bool frontend_driver_has_gamemode(void)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   return frontend && frontend->set_gamemode;
}

bool frontend_driver_set_gamemode(bool on)
{
   frontend_state_t *frontend_st   = &frontend_driver_st;
   frontend_ctx_driver_t *frontend = frontend_st->current_frontend_ctx;
   if (frontend && frontend->set_gamemode)
      return frontend->set_gamemode(on);
   return false;
}

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <string/stdstring.h>

#include "../configuration.h"
#include "../list_special.h"
#include "../verbosity.h"

#include "ui_companion_driver.h"
#ifdef __MACH__
#include <TargetConditionals.h>
#endif

static ui_companion_driver_t ui_companion_null = {
   NULL, /* init */
   NULL, /* deinit */
   NULL, /* toggle */
   NULL, /* iterate */
   NULL, /* event_command */
   NULL, /* notify_refresh */
   NULL, /* msg_queue_push */
   NULL, /* render_messagebox */
   NULL, /* get_main_window */
   NULL, /* log_msg */
   NULL, /* is_active */
   NULL, /* get_app_icons */
   NULL, /* set_app_icon */
   NULL, /* get_app_icon_texture */
   NULL, /* browser_window */
   NULL, /* msg_window */
   NULL, /* window */
   NULL, /* application */
   "null", /* ident */
};

/* Platform drivers: OS glue. Exactly one is active, chosen by platform. */
static const ui_companion_driver_t *ui_companion_drivers[] = {
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   &ui_companion_win32,
#endif
#if TARGET_OS_OSX
   &ui_companion_cocoa,
#endif
#if TARGET_OS_IPHONE
   &ui_companion_cocoatouch,
#endif
   &ui_companion_null,
   NULL
};

/* Desktop companion ("WIMP") drivers: the playlist / content browser
 * window. Selected by settings->arrays.ui_companion_driver. Order is
 * preference order for the default. */
static const ui_companion_driver_t *ui_companion_wimp_drivers[] = {
#ifdef HAVE_QT
   &ui_companion_qt,
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   &ui_companion_wimp_win32,
#endif
#if defined(HAVE_COCOA) && TARGET_OS_OSX
   &ui_companion_wimp_cocoa,
#endif
   &ui_companion_null,
   NULL
};

static uico_driver_state_t uico_driver_st = {0}; /* double alignment */

uico_driver_state_t *uico_state_get_ptr(void)
{
   return &uico_driver_st;
}

uint8_t ui_companion_get_flags(void)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   if (!uico_st)
	   return 0;
   return uico_st->flags;
}

/* --- Desktop companion driver selection ------------------------------ */

const ui_companion_driver_t *ui_companion_wimp_find_driver(const char *ident)
{
   unsigned i;
   for (i = 0; ui_companion_wimp_drivers[i]; i++)
   {
      if (string_is_equal(ui_companion_wimp_drivers[i]->ident, ident))
         return ui_companion_wimp_drivers[i];
   }
   return NULL;
}

const char *ui_companion_wimp_find_ident(int idx)
{
   int i;
   if (idx < 0)
      return NULL;
   for (i = 0; ui_companion_wimp_drivers[i]; i++)
   {
      if (i == idx)
         return ui_companion_wimp_drivers[i]->ident;
   }
   return NULL;
}

const char *config_get_ui_companion_driver_options(void)
{
   return char_list_new_special(STRING_LIST_UI_COMPANION_DRIVERS, NULL);
}

const char *config_get_default_ui_companion(void)
{
   return ui_companion_wimp_drivers[0]->ident;
}

#ifdef HAVE_COMPANION_WIMP
/* Resolve the configured desktop companion driver. An unknown ident
 * falls back to the default (first in the table) so a config written
 * by a build with a different driver set still gets a companion. */
static const ui_companion_driver_t *ui_companion_wimp_select(void)
{
   settings_t *settings            = config_get_ptr();
   const ui_companion_driver_t *drv = NULL;

   if (settings && !string_is_empty(settings->arrays.ui_companion_driver))
      drv = ui_companion_wimp_find_driver(settings->arrays.ui_companion_driver);
   if (!drv)
      drv = ui_companion_wimp_drivers[0];
   return drv;
}

static bool ui_companion_wimp_init(uico_driver_state_t *uico_st)
{
   if (uico_st->flags & UICO_ST_FLAG_WIMP_IS_INITED)
      return true;
   if (!uico_st->wimp)
      uico_st->wimp = ui_companion_wimp_select();
   if (!uico_st->wimp || !uico_st->wimp->init)
      return false;
   uico_st->wimp_data = uico_st->wimp->init();
   if (!uico_st->wimp_data)
      return false;
   uico_st->flags |= UICO_ST_FLAG_WIMP_IS_INITED;
   return true;
}
#endif

/* --- Lifecycle -------------------------------------------------------- */

void ui_companion_event_command(enum event_command action)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->drv;
   if (ui && ui->event_command)
      ui->event_command(uico_st->data, action);
#ifdef HAVE_COMPANION_WIMP
   if (     (uico_st->flags & UICO_ST_FLAG_WIMP_IS_INITED)
         && uico_st->wimp && uico_st->wimp->event_command)
      uico_st->wimp->event_command(uico_st->wimp_data, action);
#endif
}

void ui_companion_driver_deinit(void)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->drv;

   if (!ui)
      return;
   if (ui->deinit)
      ui->deinit(uico_st->data);

   ui_companion_driver_wimp_deinit();
   uico_st->data       = NULL;
}

void ui_companion_driver_wimp_deinit(void)
{
#ifdef HAVE_COMPANION_WIMP
   uico_driver_state_t *uico_st = &uico_driver_st;
   if (uico_st->flags & UICO_ST_FLAG_WIMP_IS_INITED)
   {
      if (uico_st->wimp && uico_st->wimp->deinit)
         uico_st->wimp->deinit(uico_st->wimp_data);
      uico_st->wimp_data = NULL;
      uico_st->flags    &= ~UICO_ST_FLAG_WIMP_IS_INITED;
   }
#endif
}

void ui_companion_driver_toggle(
      bool desktop_menu_enable,
      bool ui_companion_toggle,
      bool force)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   if (uico_st && uico_st->drv && uico_st->drv->toggle)
      uico_st->drv->toggle(uico_st->data, false);

#ifdef HAVE_COMPANION_WIMP
   if (desktop_menu_enable)
   {
      if (ui_companion_toggle || force)
         ui_companion_wimp_init(uico_st);

      if (     (uico_st->flags & UICO_ST_FLAG_WIMP_IS_INITED)
            && uico_st->wimp && uico_st->wimp->toggle)
         uico_st->wimp->toggle(uico_st->wimp_data, force);
   }
#endif
}

void ui_companion_driver_wimp_iterate(void)
{
#ifdef HAVE_COMPANION_WIMP
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->wimp;
   if (!ui || !(uico_st->flags & UICO_ST_FLAG_WIMP_IS_INITED))
      return;
   if (ui->iterate)
      ui->iterate(uico_st->wimp_data);
   /* A companion that owns its own toolkit event loop (Qt) pumps it
    * here; the native companions leave this NULL and ride the platform
    * driver's pump. */
   if (ui->application && ui->application->process_events)
      ui->application->process_events();
#endif
}

bool ui_companion_driver_wimp_exiting(void)
{
#ifdef HAVE_COMPANION_WIMP
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->wimp;
   if (ui && ui->application)
      return ui->application->exiting;
#endif
   return false;
}

void ui_companion_driver_wimp_quit(void)
{
#ifdef HAVE_COMPANION_WIMP
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->wimp;
   if (ui && ui->application && ui->application->quit)
      ui->application->quit();
#endif
}

void ui_companion_driver_init_first(
      bool desktop_menu_enable,
      bool ui_companion_toggle,
      unsigned ui_companion_start_on_boot
      )
{
   uico_driver_state_t *uico_st        = &uico_driver_st;
#ifdef HAVE_COMPANION_WIMP
   uico_st->wimp                       = ui_companion_wimp_select();
   /* Defer desktop companion initialization until the desktop menu
    * is actually toggled on. Constructing QApplication at startup
    * (which happens here only when desktop_menu_enable is set) aborts
    * the process on headless / KMS / Wayland-only systems with no
    * usable Qt platform plugin, as QApplication calls qFatal()
    * internally on platform-plugin init failure. This matches the
    * pre-1.21 behaviour for both Qt5 and Qt6. */
   if (desktop_menu_enable && ui_companion_toggle)
      ui_companion_wimp_init(uico_st);
#endif
   uico_st->drv                        = (ui_companion_driver_t*)ui_companion_drivers[0];

   if (!uico_st->drv)
      return;
   if (!ui_companion_start_on_boot)
      return;
   if (uico_st->drv->init)
      uico_st->data = uico_st->drv->init();

   ui_companion_driver_toggle(desktop_menu_enable,
         ui_companion_toggle, false);
}

void ui_companion_driver_notify_refresh(void)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->drv;
   if (!ui)
      return;
   if (ui->notify_refresh)
      ui->notify_refresh(uico_st->data);

#ifdef HAVE_COMPANION_WIMP
   if (config_get_ptr()->bools.desktop_menu_enable)
      if (     (uico_st->flags & UICO_ST_FLAG_WIMP_IS_INITED)
            && uico_st->wimp && uico_st->wimp->notify_refresh)
         uico_st->wimp->notify_refresh(uico_st->wimp_data);
#endif
}

const ui_msg_window_t *ui_companion_driver_get_msg_window_ptr(void)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->drv;
   if (ui)
      return ui->msg_window;
   return NULL;
}

const ui_window_t *ui_companion_driver_get_window_ptr(void)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->drv;
   if (ui)
      return ui->window;
   return NULL;
}

const ui_browser_window_t *ui_companion_driver_get_browser_window_ptr(void)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->drv;
   if (ui)
      return ui->browser_window;
   return NULL;
}

void ui_companion_driver_msg_queue_push(
      const char *msg, unsigned priority,
      unsigned duration, bool flush)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->drv;

   if (ui && ui->msg_queue_push)
      ui->msg_queue_push(uico_st->data, msg, priority, duration, flush);

#ifdef HAVE_COMPANION_WIMP
   if (config_get_ptr()->bools.desktop_menu_enable)
      if (     (uico_st->flags & UICO_ST_FLAG_WIMP_IS_INITED)
            && uico_st->wimp && uico_st->wimp->msg_queue_push)
         uico_st->wimp->msg_queue_push(
               uico_st->wimp_data,
               msg, priority, duration, flush);
#endif
}

void *ui_companion_driver_get_main_window(void)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->drv;
   if (!ui || !ui->get_main_window)
      return NULL;
   return ui->get_main_window(uico_st->data);
}

const char *ui_companion_driver_get_ident(void)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->drv;
   if (ui)
      return ui->ident;
   return "null";
}

bool ui_companion_driver_log_active(void)
{
#ifdef HAVE_COMPANION_WIMP
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->wimp;
   settings_t *settings            = config_get_ptr();
   if (!settings || !settings->bools.desktop_menu_enable)
      return false;
   return ui && ui->log_msg && uico_st->wimp_data
      && (uico_st->flags & UICO_ST_FLAG_WIMP_IS_INITED)
      && ui->is_active && ui->is_active(uico_st->wimp_data);
#else
   return false;
#endif
}

void ui_companion_driver_log_msg(const char *msg)
{
#ifdef HAVE_COMPANION_WIMP
   /* The caller has just asked ui_companion_driver_log_active(); do not
    * repeat the setting lookup and the is_active() round trip per line.
    * Only the pointers are re-checked, in case the companion was torn
    * down between the two calls. */
   uico_driver_state_t *uico_st = &uico_driver_st;
   if (uico_st->wimp && uico_st->wimp->log_msg && uico_st->wimp_data)
      uico_st->wimp->log_msg(uico_st->wimp_data, msg);
#endif
}

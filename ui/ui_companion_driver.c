/*  RetroArch - A frontend for libretro.
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

#include <boolean.h>
#include <string/stdstring.h>

#include "../configuration.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "ui_companion_driver.h"

static const ui_companion_driver_t *ui_companion_drivers[] = {
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   &ui_companion_win32,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
   &ui_companion_cocoa,
#endif
#ifdef HAVE_COCOATOUCH
   &ui_companion_cocoatouch,
#endif
   &ui_companion_null,
   NULL
};

static bool main_ui_companion_is_on_foreground = false;
static const ui_companion_driver_t *ui_companion = NULL;
static void *ui_companion_data = NULL;

#ifdef HAVE_QT
static void *ui_companion_qt_data = NULL;
static bool qt_is_inited = false;
#endif

/**
 * ui_companion_find_driver:
 * @ident               : Identifier name of driver to find.
 *
 * Finds driver with @ident. Does not initialize.
 *
 * Returns: pointer to driver if successful, otherwise NULL.
 **/
const ui_companion_driver_t *ui_companion_find_driver(const char *ident)
{
   unsigned i;

   for (i = 0; ui_companion_drivers[i]; i++)
   {
      if (string_is_equal(ui_companion_drivers[i]->ident, ident))
         return ui_companion_drivers[i];
   }

   return NULL;
}

void ui_companion_set_foreground(unsigned enable)
{
   main_ui_companion_is_on_foreground = enable;
}

bool ui_companion_is_on_foreground(void)
{
   return main_ui_companion_is_on_foreground;
}

/**
 * ui_companion_init_first:
 *
 * Finds first suitable driver and initialize.
 *
 * Returns: pointer to first suitable driver, otherwise NULL.
 **/
const ui_companion_driver_t *ui_companion_init_first(void)
{
   return ui_companion_drivers[0];
}

const ui_companion_driver_t *ui_companion_get_ptr(void)
{
   return ui_companion;
}

void ui_companion_event_command(enum event_command action)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();

   if (ui && ui->event_command)
      ui->event_command(ui_companion_data, action);
#ifdef HAVE_QT
   if (ui_companion_qt.toggle && qt_is_inited)
      ui_companion_qt.event_command(ui_companion_qt_data, action);
#endif
}

void ui_companion_driver_deinit(void)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();

   if (!ui)
      return;
   if (ui->deinit)
      ui->deinit(ui_companion_data);

#ifdef HAVE_QT
   if (qt_is_inited)
   {
      ui_companion_qt.deinit(ui_companion_qt_data);
      ui_companion_qt_data = NULL;
   }
#endif
   ui_companion_data = NULL;
}

void ui_companion_driver_init_first(void)
{
   settings_t *settings = config_get_ptr();

   ui_companion = (ui_companion_driver_t*)ui_companion_init_first();

#ifdef HAVE_QT
   if (settings->bools.desktop_menu_enable && settings->bools.ui_companion_toggle)
   {
      ui_companion_qt_data = ui_companion_qt.init();
      qt_is_inited = true;
   }
#endif

   if (ui_companion)
   {
      if (settings->bools.ui_companion_start_on_boot)
      {
         if (ui_companion->init)
            ui_companion_data = ui_companion->init();

         ui_companion_driver_toggle(false);
      }
   }
}

void ui_companion_driver_toggle(bool force)
{
#ifdef HAVE_QT
   settings_t *settings = config_get_ptr();
#endif

   if (ui_companion && ui_companion->toggle)
      ui_companion->toggle(ui_companion_data, false);

#ifdef HAVE_QT
   if (settings->bools.desktop_menu_enable)
   {
      if ((settings->bools.ui_companion_toggle || force) && !qt_is_inited)
      {
         ui_companion_qt_data = ui_companion_qt.init();
         qt_is_inited = true;
      }

      if (ui_companion_qt.toggle && qt_is_inited)
         ui_companion_qt.toggle(ui_companion_qt_data, force);
   }
#endif
}

void ui_companion_driver_notify_refresh(void)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
#ifdef HAVE_QT
   settings_t            *settings = config_get_ptr();
#endif

   if (!ui)
      return;
   if (ui->notify_refresh)
      ui->notify_refresh(ui_companion_data);
#ifdef HAVE_QT
   if (settings->bools.desktop_menu_enable)
      if (ui_companion_qt.notify_refresh && qt_is_inited)
         ui_companion_qt.notify_refresh(ui_companion_qt_data);
#endif
}

void ui_companion_driver_notify_list_loaded(file_list_t *list, file_list_t *menu_list)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui)
      return;
   if (ui->notify_list_loaded)
      ui->notify_list_loaded(ui_companion_data, list, menu_list);
}

void ui_companion_driver_notify_content_loaded(void)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui)
      return;
   if (ui->notify_content_loaded)
      ui->notify_content_loaded(ui_companion_data);
}

void ui_companion_driver_free(void)
{
   ui_companion = NULL;
}

const ui_msg_window_t *ui_companion_driver_get_msg_window_ptr(void)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui)
      return NULL;
   return ui->msg_window;
}

const ui_window_t *ui_companion_driver_get_window_ptr(void)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui)
      return NULL;
   return ui->window;
}

const ui_browser_window_t *ui_companion_driver_get_browser_window_ptr(void)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui)
      return NULL;
   return ui->browser_window;
}

#ifdef HAVE_QT
const ui_application_t *ui_companion_driver_get_qt_application_ptr(void)
{
   return ui_companion_qt.application;
}
#endif

const ui_application_t *ui_companion_driver_get_application_ptr(void)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui)
      return NULL;
   return ui->application;
}

void ui_companion_driver_msg_queue_push(const char *msg, unsigned priority, unsigned duration, bool flush)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
#ifdef HAVE_QT
   settings_t *settings = config_get_ptr();
#endif

   if (ui && ui->msg_queue_push)
      ui->msg_queue_push(ui_companion_data, msg, priority, duration, flush);
#ifdef HAVE_QT
   if (settings->bools.desktop_menu_enable)
      if (ui_companion_qt.msg_queue_push && qt_is_inited)
         ui_companion_qt.msg_queue_push(ui_companion_qt_data, msg, priority, duration, flush);
#endif
}

void *ui_companion_driver_get_main_window(void)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui || !ui->get_main_window)
      return NULL;
   return ui->get_main_window(ui_companion_data);
}

const char *ui_companion_driver_get_ident(void)
{
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui)
      return "null";
   return ui->ident;
}

void ui_companion_driver_log_msg(const char *msg)
{
#ifdef HAVE_QT
   settings_t *settings = config_get_ptr();

   if (settings && settings->bools.desktop_menu_enable)
      if (ui_companion_qt_data && qt_is_inited)
         ui_companion_qt.log_msg(ui_companion_qt_data, msg);
#endif
}

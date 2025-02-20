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

#include "../list_special.h"
#include "../verbosity.h"

#include "ui_companion_driver.h"

static ui_companion_driver_t ui_companion_null = {
   NULL, /* init */
   NULL, /* deinit */
   NULL, /* toggle */
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

static const ui_companion_driver_t *ui_companion_drivers[] = {
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   &ui_companion_win32,
#endif
#if defined(OSX)
   &ui_companion_cocoa,
#endif
#if defined(IOS)
   &ui_companion_cocoatouch,
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

void ui_companion_event_command(enum event_command action)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   const ui_companion_driver_t *ui = uico_st->drv;
   if (ui && ui->event_command)
      ui->event_command(uico_st->data, action);
#ifdef HAVE_QT
   if (ui_companion_qt.toggle && (uico_st->flags & UICO_ST_FLAG_QT_IS_INITED))
      ui_companion_qt.event_command(uico_st->qt_data, action);
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

#ifdef HAVE_QT
   if (uico_st->flags & UICO_ST_FLAG_QT_IS_INITED)
   {
      ui_companion_qt.deinit(uico_st->qt_data);
      uico_st->qt_data = NULL;
   }
#endif
   uico_st->data       = NULL;
}

void ui_companion_driver_toggle(
      bool desktop_menu_enable,
      bool ui_companion_toggle,
      bool force)
{
   uico_driver_state_t *uico_st    = &uico_driver_st;
   if (uico_st && uico_st->drv && uico_st->drv->toggle)
      uico_st->drv->toggle(uico_st->data, false);

#ifdef HAVE_QT
   if (desktop_menu_enable)
   {
      if ((ui_companion_toggle || force) && (!(uico_st->flags & UICO_ST_FLAG_QT_IS_INITED)))
      {
         uico_st->qt_data          = ui_companion_qt.init();
         uico_st->flags           |= UICO_ST_FLAG_QT_IS_INITED;
      }

      if (ui_companion_qt.toggle && (uico_st->flags & UICO_ST_FLAG_QT_IS_INITED))
         ui_companion_qt.toggle(uico_st->qt_data, force);
   }
#endif
}

void ui_companion_driver_init_first(
      bool desktop_menu_enable,
      bool ui_companion_toggle,
      unsigned ui_companion_start_on_boot
      )
{
   uico_driver_state_t *uico_st        = &uico_driver_st;
#ifdef HAVE_QT
   if (desktop_menu_enable && ui_companion_toggle)
   {
      uico_st->qt_data                 = ui_companion_qt.init();
      uico_st->flags                  |= UICO_ST_FLAG_QT_IS_INITED;
   }
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

#ifdef HAVE_QT
   if (config_get_ptr()->bools.desktop_menu_enable)
      if (ui_companion_qt.notify_refresh && (uico_st->flags & UICO_ST_FLAG_QT_IS_INITED))
         ui_companion_qt.notify_refresh(uico_st->qt_data);
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

#ifdef HAVE_QT
   if (config_get_ptr()->bools.desktop_menu_enable)
      if (ui_companion_qt.msg_queue_push && (uico_st->flags & UICO_ST_FLAG_QT_IS_INITED))
         ui_companion_qt.msg_queue_push(
               uico_st->qt_data,
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

void ui_companion_driver_log_msg(const char *msg)
{
#ifdef HAVE_QT
   uico_driver_state_t *uico_st= &uico_driver_st;
   bool window_is_active       = uico_st->qt_data && (uico_st->flags & UICO_ST_FLAG_QT_IS_INITED)
      && ui_companion_qt.is_active(uico_st->qt_data);
   if (config_get_ptr()->bools.desktop_menu_enable)
      if (window_is_active)
         ui_companion_qt.log_msg(uico_st->qt_data, msg);
#endif
}

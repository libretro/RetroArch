/*  RetroArch - A frontend for libretro.
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

#include <string.h>

#include <boolean.h>

#include "../configuration.h"
#include "../driver.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static bool main_ui_companion_is_on_foreground;

static const ui_companion_driver_t *ui_companion_drivers[] = {
#if defined(_WIN32) && !defined(_XBOX)
   &ui_companion_win32,
#endif
#ifdef HAVE_COCOA
   &ui_companion_cocoa,
#endif
#ifdef HAVE_COCOATOUCH
   &ui_companion_cocoatouch,
#endif
#ifdef HAVE_QT
   &ui_companion_qt,
#endif
   &ui_companion_null,
   NULL
};

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
      if (!strcmp(ui_companion_drivers[i]->ident, ident))
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
   unsigned i;

   for (i = 0; ui_companion_drivers[i]; i++)
      return ui_companion_drivers[i];

   return NULL;
}

const ui_companion_driver_t *ui_companion_get_ptr(void)
{
   driver_t *driver        = driver_get_ptr();
   if (!driver)
      return NULL;
   return driver->ui_companion;
}

void ui_companion_event_command(enum event_command action)
{
   driver_t *driver        = driver_get_ptr();
   const ui_companion_driver_t *ui = ui_companion_get_ptr();

   if (driver && ui && ui->event_command)
      ui->event_command(driver->ui_companion_data, action);
}

void ui_companion_driver_deinit(void)
{
   driver_t *driver        = driver_get_ptr();
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui)
      return;
   if (ui->deinit)
      ui->deinit(driver->ui_companion_data);
}

void ui_companion_driver_init_first(void)
{
   settings_t *settings    = config_get_ptr();
   driver_t *driver        = driver_get_ptr();
   if (driver)
      driver->ui_companion = (ui_companion_driver_t*)ui_companion_init_first();

   if (driver->ui_companion && driver->ui_companion->toggle)
   {
      if (settings->ui.companion_start_on_boot)
         driver->ui_companion->toggle(driver->ui_companion_data);
   }
}

void ui_companion_driver_notify_refresh(void)
{
   driver_t *driver        = driver_get_ptr();
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui)
      return;
   if (ui->notify_refresh)
      ui->notify_refresh(driver->ui_companion_data);
}

void ui_companion_driver_notify_list_loaded(file_list_t *list, file_list_t *menu_list)
{
   driver_t *driver        = driver_get_ptr();
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui)
      return;
   if (ui->notify_list_loaded)
      ui->notify_list_loaded(driver->ui_companion_data, list, menu_list);
}

void ui_companion_driver_notify_content_loaded(void)
{
   driver_t *driver        = driver_get_ptr();
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
   if (!ui)
      return;
   if (ui->notify_content_loaded)
      ui->notify_content_loaded(driver->ui_companion_data);
}

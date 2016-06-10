/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "wrapper/wrapper.h"
#include "../ui_qt.h"

#include "../../ui_companion_driver.h"

static void ui_window_qt_destroy(void *data)
{
   ui_window_qt_t *window = (ui_window_qt_t*)data;

   delete window->qtWindow;
}

static void ui_window_qt_set_focused(void *data)
{
   ui_window_qt_t *window = (ui_window_qt_t*)data;

   window->qtWindow->raise();
   window->qtWindow->activateWindow();
}

static void ui_window_qt_set_visible(void *data,
        bool set_visible)
{
   /* TODO/FIXME */
}

static void ui_window_qt_set_title(void *data, char *buf)
{
   ui_window_qt_t *window = (ui_window_qt_t*)data;

   window->qtWindow->setWindowTitle(QString::fromUtf8(buf));
}

static void ui_window_qt_set_droppable(void *data, bool droppable)
{
   ui_window_qt_t *window = (ui_window_qt_t*)data;

  window->qtWindow->setAcceptDrops(droppable);
}

static bool ui_window_qt_focused(void *data)
{
   ui_window_qt_t *window = (ui_window_qt_t*)data;
   return window->qtWindow->isActiveWindow() && !window->qtWindow->isMinimized();
}

const ui_window_t ui_window_qt = {
   ui_window_qt_destroy,
   ui_window_qt_set_focused,
   ui_window_qt_set_visible,
   ui_window_qt_set_title,
   ui_window_qt_set_droppable,
   ui_window_qt_focused,
   "qt"
};

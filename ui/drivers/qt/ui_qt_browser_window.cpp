/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <QtWidgets/QFileDialog>
#include <QtCore/QString>

#include "../../ui_companion_driver.h"

static bool ui_browser_window_qt_open(ui_browser_window_state_t *state)
{
   return true;
}

static bool ui_browser_window_qt_save(ui_browser_window_state_t *state)
{
   return false;
}

ui_browser_window_t ui_browser_window_qt = {
   ui_browser_window_qt_open,
   ui_browser_window_qt_save,
   "qt"
};

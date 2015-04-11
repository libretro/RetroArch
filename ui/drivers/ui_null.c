/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Ali Bouhlel
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
#include <string.h>

#include <file/file_path.h>
#include "../ui_companion_driver.h"

static void ui_companion_null_deinit(void *data)
{
   (void)data;
}

static void ui_companion_null_init(void *data)
{
   (void)data;
}

const ui_companion_driver_t ui_companion_null = {
   ui_companion_null_init,
   ui_companion_null_deinit,
   NULL,
   NULL,
   "null",
};

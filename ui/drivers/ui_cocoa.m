/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "cocoa/cocoa_common.h"
#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include "../ui_companion_driver.h"

typedef struct ui_companion_cocoa
{
   void *empty;
} ui_companion_cocoa_t;

static void ui_companion_cocoa_notify_content_loaded(void *data)
{
    (void)data;
}

static void ui_companion_cocoa_toggle(void *data)
{
   (void)data;
}

static int ui_companion_cocoa_iterate(void *data, unsigned action)
{
   (void)data;

   return 0;
}

static void ui_companion_cocoa_deinit(void *data)
{
   ui_companion_cocoa_t *handle = (ui_companion_cocoa_t*)data;

   if (handle)
      free(handle);
}

static void *ui_companion_cocoa_init(void)
{
   ui_companion_cocoa_t *handle = (ui_companion_cocoa_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   return handle;
}

static void ui_companion_cocoa_event_command(void *data, unsigned cmd)
{
   ui_companion_cocoa_t *handle = (ui_companion_cocoa_t*)data;

   if (!handle)
      return;

   event_command(cmd);
}

const ui_companion_driver_t ui_companion_cocoa = {
   ui_companion_cocoa_init,
   ui_companion_cocoa_deinit,
   ui_companion_cocoa_iterate,
   ui_companion_cocoa_toggle,
   ui_companion_cocoa_event_command,
   ui_companion_cocoa_notify_content_loaded,
   "cocoa",
};

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

#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "general.h"
#include "system.h"

#include <file/file_path.h>
#include <rthreads/rthreads.h>
#include "../ui_companion_driver.h"

#include "qt/wrapper/wrapper.h"

struct Wimp* wimp;
char* args[] = {""};
settings_t *settings;

typedef struct ui_companion_qt
{
   volatile bool quit;
   slock_t *lock;
   sthread_t *thread;
} ui_companion_qt_t;

static void qt_thread(void *data)
{

   ui_companion_qt_t *handle = (ui_companion_qt_t*)data;
   wimp = ctrWimp(0, NULL);
   if(wimp)
   {
      GetSettings(wimp, settings);
      CreateMainWindow(wimp);
   }

   return;
}

static void ui_companion_qt_deinit(void *data)
{
   ui_companion_qt_t *handle = (ui_companion_qt_t*)data;

   if (!handle)
      return;

   slock_free(handle->lock);
   sthread_join(handle->thread);

   free(handle);
}

static void *ui_companion_qt_init(void)
{
   ui_companion_qt_t *handle = (ui_companion_qt_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;
   settings = config_get_ptr();
   handle->lock   = slock_new();
   handle->thread = sthread_create(qt_thread, handle);
   if (!handle->thread)
   {
      slock_free(handle->lock);
      free(handle);
      return NULL;
   }

   return handle;
}

static int ui_companion_qt_iterate(void *data, unsigned action)
{
   (void)data;
   (void)action;
   return 0;
}

static void ui_companion_qt_notify_content_loaded(void *data)
{
   (void)data;
}

static void ui_companion_qt_toggle(void *data)
{
   ui_companion_qt_init();
}

static void ui_companion_qt_event_command(void *data, enum event_command cmd)
{
   ui_companion_qt_t *handle = (ui_companion_qt_t*)data;

   if (!handle)
      return;

   slock_lock(handle->lock);
   command_event(cmd, NULL);
   slock_unlock(handle->lock);
}

static void ui_companion_qt_notify_list_pushed(void *data, file_list_t *list,
   file_list_t *menu_list)
{
   (void)data;
   (void)list;
   (void)menu_list;
}

const ui_companion_driver_t ui_companion_qt = {
   ui_companion_qt_init,
   ui_companion_qt_deinit,
   ui_companion_qt_iterate,
   ui_companion_qt_toggle,
   ui_companion_qt_event_command,
   ui_companion_qt_notify_content_loaded,
   ui_companion_qt_notify_list_pushed,
   NULL,
   NULL,
   NULL,
   "qt",
};

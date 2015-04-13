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

#include "../../apple/common/RetroArch_Apple.h"
#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include "../ui_companion_driver.h"

typedef struct ui_companion_cocoatouch
{
   void *empty;
} ui_companion_cocoatouch_t;

static void ui_companion_cocoatouch_switch_to_ios(void *data)
{
   RetroArch_iOS *ap  = NULL;
   runloop_t *runloop = rarch_main_get_ptr();
    
   (void)data;

   if (!apple_platform)
      return;
    
   ap = (RetroArch_iOS *)apple_platform;
   runloop->is_idle = true;
   [ap showPauseMenu:ap];
}

static void ui_companion_cocoatouch_notify_content_loaded(void *data)
{
   RetroArch_iOS *ap = (RetroArch_iOS *)apple_platform;
   
    (void)data;
    
   if (ap)
      [ap showGameView];
}

static void ui_companion_cocoatouch_toggle(void *data)
{
   RetroArch_iOS *ap   = (RetroArch_iOS *)apple_platform;

   (void)data;

   if (ap)
      [ap toggleUI];
}

static int ui_companion_cocoatouch_iterate(void *data, unsigned action)
{
   (void)data;

   ui_companion_cocoatouch_switch_to_ios(data);

   return 0;
}

static void ui_companion_cocoatouch_deinit(void *data)
{
   ui_companion_cocoatouch_t *handle = (ui_companion_cocoatouch_t*)data;

   if (handle)
      free(handle);
}

static void *ui_companion_cocoatouch_init(void)
{
   ui_companion_cocoatouch_t *handle = (ui_companion_cocoatouch_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   return handle;
}

static void ui_companion_cocoatouch_event_command(void *data, unsigned cmd)
{
   ui_companion_cocoatouch_t *handle = (ui_companion_cocoatouch_t*)data;

   if (!handle)
      return;

   event_command(cmd);
}

const ui_companion_driver_t ui_companion_cocoatouch = {
   ui_companion_cocoatouch_init,
   ui_companion_cocoatouch_deinit,
   ui_companion_cocoatouch_iterate,
   ui_companion_cocoatouch_toggle,
   ui_companion_cocoatouch_event_command,
   ui_companion_cocoatouch_notify_content_loaded,
   "cocoatouch",
};

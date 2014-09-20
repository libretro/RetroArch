/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2014 - Daniel De Matteis
 * Copyright (C) 2012-2014 - Jason Fetters
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "../menu/menu_common.h"
#include "../../settings_data.h"

#include "../frontend.h"

#include <stdint.h>
#include "../../boolean.h"
#include <stddef.h>
#include <string.h>

static CFRunLoopObserverRef iterate_observer;

static void do_iteration(void)
{
   if (!(g_extern.main_is_init && !g_extern.is_paused))
      return;

   if (main_entry_decide(0, NULL, NULL))
   {
      main_exit(NULL);
      return;
   }

   CFRunLoopWakeUp(CFRunLoopGetMain());
}

void apple_start_iteration(void)
{
    iterate_observer = CFRunLoopObserverCreate(0, kCFRunLoopBeforeWaiting,
          true, 0, (CFRunLoopObserverCallBack)do_iteration, 0);
    CFRunLoopAddObserver(CFRunLoopGetMain(), iterate_observer,
          kCFRunLoopCommonModes);
}

void apple_stop_iteration(void)
{
    CFRunLoopObserverInvalidate(iterate_observer);
    CFRelease(iterate_observer);
    iterate_observer = 0;
}

extern void apple_rarch_exited(void);

static void frontend_apple_shutdown(bool unused)
{
    apple_rarch_exited();
}

static int frontend_apple_get_rating(void)
{
   /* TODO/FIXME - look at unique identifier per device and 
    * determine rating for some */
   return -1;
}
const frontend_ctx_driver_t frontend_ctx_apple = {
   NULL,                         /* environment_get */
   NULL,                         /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* process_events */
   NULL,                         /* exec */
   frontend_apple_shutdown,      /* shutdown */
   NULL,                         /* get_name */
   frontend_apple_get_rating,    /* get_rating */
   "apple",
};

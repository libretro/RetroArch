/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <screen/screen.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <bps/bps.h>
#include <bps/event.h>

#include "../playbook/src/bbutil.h"

void handle_screen_event(bps_event_t *event)
{
   screen_event_t screen_event = screen_event_get_event(event);

   int screen_val;
   screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &screen_val);

   switch (screen_val)
   {
      case SCREEN_EVENT_MTOUCH_TOUCH:
      case SCREEN_EVENT_MTOUCH_MOVE:
      case SCREEN_EVENT_MTOUCH_RELEASE:
         break;
   }
}

int rarch_main(int argc, char *argv[])
{
   bps_initialize();   //Initialize BPS library

   int init_ret;
   if ((init_ret = rarch_main_init(argc, argv))) return init_ret;
   rarch_init_msg_queue();
   while ((g_extern.is_paused && !g_extern.is_oneshot) ? rarch_main_idle_iterate() : rarch_main_iterate());
   rarch_main_deinit();
   rarch_deinit_msg_queue();

#ifdef PERF_TEST
   rarch_perf_log();
#endif

   rarch_main_clear_state();

   bps_shutdown();     //Shut down BPS library

   return 0;
}

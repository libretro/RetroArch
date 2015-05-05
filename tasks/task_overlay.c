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

#include <retro_miscellaneous.h>

#include "../driver.h"
#include "../runloop.h"
#include "../runloop_data.h"
#include "tasks.h"

void rarch_main_data_overlay_image_upload_iterate(bool is_thread, void *data)
{
   data_runloop_t *runloop = (data_runloop_t*)data;
   driver_t *driver = driver_get_ptr();

   if (rarch_main_is_idle())
      return;
   if (!driver->overlay || !runloop)
      return;

#ifdef HAVE_THREADS
   if (is_thread)
      slock_lock(runloop->overlay_lock);
#endif

   switch (driver->overlay->state)
   {
      case OVERLAY_STATUS_DEFERRED_LOADING:
         input_overlay_load_overlays_iterate(driver->overlay);
         break;
      default:
         break;
   }

#ifdef HAVE_THREADS
   if (is_thread)
      slock_unlock(runloop->overlay_lock);
#endif
}

void rarch_main_data_overlay_iterate(bool is_thread, void *data)
{
   data_runloop_t *runloop = (data_runloop_t*)data;
   driver_t *driver = NULL;
   
   if (rarch_main_is_idle())
      return;

#ifdef HAVE_THREADS
   if (is_thread)
      slock_lock(runloop->overlay_lock);
#endif

   driver = driver_get_ptr();

   if (!driver || !driver->overlay)
      goto end;

   switch (driver->overlay->state)
   {
      case OVERLAY_STATUS_DEFERRED_LOAD:
         input_overlay_load_overlays(driver->overlay);
         break;
      case OVERLAY_STATUS_NONE:
      case OVERLAY_STATUS_ALIVE:
         break;
      case OVERLAY_STATUS_DEFERRED_LOADING_RESOLVE:
         input_overlay_load_overlays_resolve_iterate(driver->overlay);
         break;
      case OVERLAY_STATUS_DEFERRED_DONE:
         input_overlay_new_done(driver->overlay);
         break;
      case OVERLAY_STATUS_DEFERRED_ERROR:
         input_overlay_free(driver->overlay);
         break;
      default:
         break;
   }

end:
#ifdef HAVE_THREADS
   if (is_thread)
      slock_unlock(runloop->overlay_lock);
#endif
}

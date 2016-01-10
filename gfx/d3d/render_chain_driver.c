/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "render_chain_driver.h"

static const renderchain_driver_t *renderchain_drivers[] = {
#ifdef HAVE_CG
   &cg_d3d9_renderchain,
#endif
#ifdef _XBOX
   &xdk_renderchain,
#endif
   &null_renderchain,
   NULL
};

bool renderchain_init_first(const renderchain_driver_t **renderchain_driver,
	void **renderchain_handle)
{
   unsigned i;

   for (i = 0; renderchain_drivers[i]; i++)
   {
      void *data = renderchain_drivers[i]->chain_new();

      if (!data)
         continue;

      *renderchain_driver = renderchain_drivers[i];
      *renderchain_handle = data;
      return true;
   }

   return false;
}

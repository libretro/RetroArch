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

#include "system.h"

static rarch_system_info_t *g_system;

static rarch_system_info_t *rarch_system_info_new(void)
{
   return (rarch_system_info_t*)calloc(1, sizeof(rarch_system_info_t)); 
}

rarch_system_info_t *rarch_system_info_get_ptr(void)
{
   if (!g_system)
      g_system = rarch_system_info_new();
   return g_system;
}

void rarch_system_info_free(void)
{
   if (!g_system)
      return;

   if (g_system->core_options)
   {
      core_option_flush(g_system->core_options);
      core_option_free(g_system->core_options);
   }

   /* No longer valid. */
   if (g_system->special)
      free(g_system->special);
   g_system->special = NULL;
   if (g_system->ports)
      free(g_system->ports);
   g_system->ports   = NULL;

   free(g_system);
   g_system = NULL;
}

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

static const input_osk_driver_t *osk_drivers[] = {
#ifdef __CELLOS_LV2__
   &input_ps3_osk,
#endif
   NULL,
};

static int find_osk_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; osk_drivers[i]; i++)
      if (strcasecmp(driver, osk_drivers[i]->ident) == 0)
         return i;
   return -1;
}

static void find_osk_driver(void)
{
   int i = find_osk_driver_index(g_settings.osk.driver);
   if (i >= 0)
      driver.osk = osk_drivers[i];
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any OSK driver named \"%s\"\n", g_settings.osk.driver);
      RARCH_LOG_OUTPUT("Available OSK drivers are:\n");
      for (d = 0; osk_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", osk_drivers[d]->ident);

      rarch_fail(1, "find_osk_driver()");
   }
}

void find_prev_osk_driver(void)
{
   int i = find_osk_driver_index(g_settings.osk.driver);
   if (i > 0)
      strlcpy(g_settings.osk.driver, osk_drivers[i - 1]->ident, sizeof(g_settings.osk.driver));
   else
      RARCH_WARN("Couldn't find any previous osk driver (current one: \"%s\").\n", g_settings.osk.driver);
}

void find_next_osk_driver(void)
{
   int i = find_osk_driver_index(g_settings.osk.driver);
   if (i >= 0 && osk_drivers[i + 1])
      strlcpy(g_settings.osk.driver, osk_drivers[i + 1]->ident, sizeof(g_settings.osk.driver));
   else
      RARCH_WARN("Couldn't find any next osk driver (current one: \"%s\").\n", g_settings.osk.driver);
}

void init_osk(void)
{
   // Resource leaks will follow if osk is initialized twice.
   if (driver.osk_data)
      return;

   find_osk_driver();

   //FIXME - refactor params later based on semantics 
   driver.osk_data = osk_init_func(0);

   if (!driver.osk_data)
   {
      RARCH_ERR("Failed to initialize OSK driver. Will continue without OSK.\n");
      g_extern.osk_active = false;
   }
}

void uninit_osk(void)
{
   if (driver.osk_data && driver.osk)
      driver.osk->free(driver.osk_data);
}

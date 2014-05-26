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

static const menu_ctx_driver_t *menu_ctx_drivers[] = {
#if defined(HAVE_RMENU)
   &menu_ctx_rmenu,
#endif
#if defined(HAVE_RMENU_XUI)
   &menu_ctx_rmenu_xui,
#endif
#if defined(HAVE_LAKKA)
   &menu_ctx_lakka,
#endif
#if defined(HAVE_RGUI)
   &menu_ctx_rgui,
#endif

   NULL // zero length array is not valid
};

const void *menu_ctx_find_driver(const char *ident)
{
   unsigned i;
   for (i = 0; menu_ctx_drivers[i]; i++)
   {
      if (strcmp(menu_ctx_drivers[i]->ident, ident) == 0)
         return menu_ctx_drivers[i];
   }

   return NULL;
}

static int find_menu_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; menu_ctx_drivers[i]; i++)
      if (strcasecmp(driver, menu_ctx_drivers[i]->ident) == 0)
         return i;
   return -1;
}

void find_prev_menu_driver(void)
{
   int i = find_menu_driver_index(g_settings.menu.driver);
   if (i > 0)
   {
      strlcpy(g_settings.menu.driver, menu_ctx_drivers[i - 1]->ident, sizeof(g_settings.menu.driver));
      driver.menu_ctx = (menu_ctx_driver_t*)menu_ctx_drivers[i - 1];
   }
   else
      RARCH_WARN("Couldn't find any previous menu driver (current one: \"%s\").\n", g_settings.menu.driver);
}

void find_next_menu_driver(void)
{
   int i = find_menu_driver_index(g_settings.menu.driver);
   if (i >= 0 && menu_ctx_drivers[i + 1])
   {
      strlcpy(g_settings.menu.driver, menu_ctx_drivers[i + 1]->ident, sizeof(g_settings.menu.driver));
      driver.menu_ctx = (menu_ctx_driver_t*)menu_ctx_drivers[i + 1];
   }
   else
      RARCH_WARN("Couldn't find any next menu driver (current one: \"%s\").\n", g_settings.menu.driver);
}

static void find_menu_driver(void)
{
   int i = find_menu_driver_index(g_settings.menu.driver);
   if (i >= 0)
      driver.menu_ctx = menu_ctx_drivers[i];
   else
   {
      unsigned d;
      RARCH_WARN("Couldn't find any menu driver named \"%s\"\n", g_settings.menu.driver);
      RARCH_LOG_OUTPUT("Available menu drivers are:\n");
      for (d = 0; menu_ctx_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", menu_ctx_drivers[d]->ident);
      RARCH_WARN("Going to default to first menu driver...\n");

      driver.menu_ctx = menu_ctx_drivers[0];

      if (!driver.menu_ctx)
         rarch_fail(1, "find_menu_driver()");
   }
}

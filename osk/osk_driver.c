/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <string.h>
#include <string/string_list.h>
#include "osk_driver.h"
#include "../driver.h"
#include "../general.h"

static const input_osk_driver_t *osk_drivers[] = {
#ifdef __CELLOS_LV2__
   &input_ps3_osk,
#endif
   &input_null_osk,
   NULL,
};

/**
 * osk_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to OSK driver at index. Can be NULL
 * if nothing found.
 **/
const void *osk_driver_find_handle(int index)
{
   const void *drv = osk_drivers[index];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * osk_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of OSK driver at index. Can be NULL
 * if nothing found.
 **/
const char *osk_driver_find_ident(int index)
{
   const input_osk_driver_t *drv = osk_drivers[index];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_osk_driver_options:
 *
 * Get an enumerated list of all OSK (onscreen keyboard) driver names,
 * separated by '|'.
 *
 * Returns: string listing of all OSK (onscreen keyboard) driver names,
 * separated by '|'.
 **/
const char* config_get_osk_driver_options(void)
{
   union string_list_elem_attr attr;
   unsigned i;
   char *options = NULL;
   int options_len = 0;
   struct string_list *options_l = string_list_new();

   attr.i = 0;

   for (i = 0; osk_driver_find_handle(i); i++)
   {
      const char *opt = osk_driver_find_ident(i);
      options_len += strlen(opt) + 1;
      string_list_append(options_l, opt, attr);
   }

   options = (char*)calloc(options_len, sizeof(char));

   string_list_join_concat(options, options_len, options_l, "|");

   string_list_free(options_l);
   options_l = NULL;

   return options;
}

/**
 * find_osk_driver:
 *
 * Find OSK (onscreen keyboard) driver.
 **/
void find_osk_driver(void)
{
   int i = find_driver_index("osk_driver", g_settings.osk.driver);
   if (i >= 0)
      driver.osk = (const input_osk_driver_t*)osk_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any OSK driver named \"%s\"\n",
            g_settings.osk.driver);
      RARCH_LOG_OUTPUT("Available OSK drivers are:\n");
      for (d = 0; osk_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", osk_driver_find_ident(d));

      RARCH_WARN("Going to default to first OSK driver...\n");
       
      driver.osk = (const input_osk_driver_t*)osk_driver_find_handle(0);
       
      if (!driver.osk)
         rarch_fail(1, "find_osk_driver()");
   }
}

void init_osk(void)
{
   /* Resource leaks will follow if osk is initialized twice. */
   if (driver.osk_data)
      return;

   find_osk_driver();

   /* FIXME - refactor params later based on semantics  */
   driver.osk_data = driver.osk->init(0);

   if (!driver.osk_data)
   {
      RARCH_ERR("Failed to initialize OSK driver. Will continue without OSK.\n");
      driver.osk_active = false;
   }
}

void uninit_osk(void)
{
   if (driver.osk_data && driver.osk && driver.osk->free)
      driver.osk->free(driver.osk_data);
   driver.osk_data = NULL;
}

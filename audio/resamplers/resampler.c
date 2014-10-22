/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "resampler.h"
#ifdef RARCH_INTERNAL
#include "../../performance.h"
#endif
#include <file/config_file_userdata.h>
#include <string.h>

static const rarch_resampler_t *resampler_drivers[] = {
   &sinc_resampler,
   &CC_resampler,
   &nearest_resampler,
   NULL,
};

static const struct resampler_config resampler_config = {
   config_userdata_get_float,
   config_userdata_get_int,
   config_userdata_get_float_array,
   config_userdata_get_int_array,
   config_userdata_get_string,
   config_userdata_free,
};

static int find_resampler_driver_index(const char *drv)
{
   unsigned i;
   for (i = 0; resampler_drivers[i]; i++)
      if (strcasecmp(drv, resampler_drivers[i]->ident) == 0)
         return i;
   return -1;
}

#if !defined(RESAMPLER_TEST) && defined(RARCH_INTERNAL)
#include "../../general.h"

void find_prev_resampler_driver(void)
{
   int i = find_resampler_driver_index(g_settings.audio.resampler);
   if (i > 0)
      strlcpy(g_settings.audio.resampler, resampler_drivers[i - 1]->ident,
            sizeof(g_settings.audio.resampler));
   else
      RARCH_WARN("Couldn't find any previous resampler driver (current one: \"%s\").\n",
            driver.resampler->ident);
}

void find_next_resampler_driver(void)
{
   int i = find_resampler_driver_index(g_settings.audio.resampler);
   if (i >= 0 && resampler_drivers[i + 1])
      strlcpy(g_settings.audio.resampler, resampler_drivers[i + 1]->ident,
            sizeof(g_settings.audio.resampler));
   else
      RARCH_WARN("Couldn't find any next resampler driver (current one: \"%s\").\n",
            driver.resampler->ident);
}
#endif

/* Resampler is used by multiple modules so avoid 
 * clobbering driver.resampler here. */

static const rarch_resampler_t *find_resampler_driver(const char *ident)
{
   int i = find_resampler_driver_index(ident);
   if (i >= 0)
      return resampler_drivers[i];
   else
   {
#ifdef RARCH_INTERNAL
      unsigned d;
      RARCH_ERR("Couldn't find any resampler driver named \"%s\"\n", ident);
      RARCH_LOG_OUTPUT("Available resampler drivers are:\n");
      for (d = 0; resampler_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", resampler_drivers[d]->ident);

      RARCH_WARN("Going to default to first resampler driver ...\n");
#endif

      return resampler_drivers[0];
   }
}

static bool resampler_append_plugs(void **re,
      const rarch_resampler_t **backend,
      double bw_ratio)
{
   resampler_simd_mask_t mask = rarch_get_cpu_features();

   *re = (*backend)->init(&resampler_config, bw_ratio, mask);

   if (!*re)
      return false;
   return true;
}

bool rarch_resampler_realloc(void **re, const rarch_resampler_t **backend,
      const char *ident, double bw_ratio)
{
   if (*re && *backend)
      (*backend)->free(*re);

   *re      = NULL;
   *backend = find_resampler_driver(ident);

   if (!resampler_append_plugs(re, backend, bw_ratio))
      goto error;

   return true;

error:
   if (!*re)
      *backend = NULL;
   return false;
}

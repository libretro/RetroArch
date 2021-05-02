/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2021 - David Guillen Fandos
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

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <lists/dir_list.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#include "cpufreq.h"

#define REFRESH_TIMEOUT  2
#define CPU_POLICIES_DIR "/sys/devices/system/cpu/cpufreq/"

static time_t last_update = 0;
static cpu_scaling_driver_t **scaling_drivers = NULL;

static bool readparse_uint32(const char *path, uint32_t *value)
{
   char *tmpbuf;
   if (!filestream_read_file(path, (void**)&tmpbuf, NULL))
      return false;
   string_remove_all_chars(tmpbuf, '\n');
   if (sscanf(tmpbuf, "%" PRIu32, value) != 1)
   {
      free(tmpbuf);
      return false;
   }
   free(tmpbuf);
   return true;
}

static struct string_list* readparse_list(const char *path)
{
   char *tmpbuf;
   struct string_list* ret;
   if (!filestream_read_file(path, (void**)&tmpbuf, NULL))
      return NULL;
   string_remove_all_chars(tmpbuf, '\n');
   ret = string_split(tmpbuf, " ");
   free(tmpbuf);
   return ret;
}

static void free_drivers(cpu_scaling_driver_t **d)
{
   if (d)
   {
      cpu_scaling_driver_t **it = d;
      while (*it)
      {
         cpu_scaling_driver_t *drv = *it++;
         if (drv->affected_cpus)
            free(drv->affected_cpus);
         if (drv->scaling_governor)
            free(drv->scaling_governor);
         if (drv->available_freqs)
            free(drv->available_freqs);
         string_list_free(drv->available_governors);

         free(drv);
      }
      free(d);
   }
}

void cpu_scaling_driver_free()
{
   if (scaling_drivers)
      free_drivers(scaling_drivers);

   scaling_drivers = NULL;
   last_update = 0;
}

cpu_scaling_driver_t **get_cpu_scaling_drivers(bool can_update)
{
   if (can_update && (time(NULL) > last_update + REFRESH_TIMEOUT ||
       !scaling_drivers))
   {
      /* Parse /sys/devices/system/cpu/cpufreq/ policies */
      int i, j, pc;
      struct string_list *policy_dir = dir_list_new(CPU_POLICIES_DIR, NULL,
        true, false, false, false);
      if (!policy_dir)
         return NULL;
      dir_list_sort(policy_dir, false);

      /* Delete the previous list of drivers */
      free_drivers(scaling_drivers);

      scaling_drivers = (cpu_scaling_driver_t**)calloc(
         (policy_dir->size + 1), sizeof(cpu_scaling_driver_t*));
      for (i = 0, pc = 0; i < policy_dir->size; i++)
      {
         uint32_t polid;
         cpu_scaling_driver_t *drv;
         struct string_list *tmplst;
         char fpath[PATH_MAX_LENGTH];
         const char *fname = strrchr(policy_dir->elems[i].data, '/');

         if (!fname)
            continue;

         /* Ensure this is a policy and get its ID */
         if (sscanf(fname, "/policy%" PRIu32, &polid) != 1)
            continue;

         drv = calloc(1, sizeof(cpu_scaling_driver_t));
         drv->policy_id = polid;

         /* Read all nodes with freq info */
         fill_pathname_join(fpath, policy_dir->elems[i].data,
            "scaling_cur_freq", sizeof(fpath));
         readparse_uint32(fpath, &drv->current_frequency);

         fill_pathname_join(fpath, policy_dir->elems[i].data,
            "cpuinfo_min_freq", sizeof(fpath));
         readparse_uint32(fpath, &drv->min_cpu_freq);

         fill_pathname_join(fpath, policy_dir->elems[i].data,
            "cpuinfo_max_freq", sizeof(fpath));
         readparse_uint32(fpath, &drv->max_cpu_freq);

         fill_pathname_join(fpath, policy_dir->elems[i].data,
            "scaling_min_freq", sizeof(fpath));
         readparse_uint32(fpath, &drv->min_policy_freq);

         fill_pathname_join(fpath, policy_dir->elems[i].data,
            "scaling_max_freq", sizeof(fpath));
         readparse_uint32(fpath, &drv->max_policy_freq);

         fill_pathname_join(fpath, policy_dir->elems[i].data,
            "scaling_available_governors", sizeof(fpath));
         drv->available_governors = readparse_list(fpath);

         fill_pathname_join(fpath, policy_dir->elems[i].data,
            "affected_cpus", sizeof(fpath));
         filestream_read_file(fpath, (void**)&drv->affected_cpus, NULL);
         string_remove_all_chars(drv->affected_cpus, '\n');

         fill_pathname_join(fpath, policy_dir->elems[i].data,
            "scaling_governor", sizeof(fpath));
         filestream_read_file(fpath, (void**)&drv->scaling_governor, NULL);
         string_remove_all_chars(drv->scaling_governor, '\n');

         fill_pathname_join(fpath, policy_dir->elems[i].data,
            "scaling_available_frequencies", sizeof(fpath));
         tmplst = readparse_list(fpath);
         if (tmplst)
         {
            drv->available_freqs = calloc(tmplst->size, sizeof(uint32_t));
            for (j = 0; j < tmplst->size; j++)
            {
               drv->available_freqs[j] = (uint32_t)atol(tmplst->elems[j].data);
            }
            string_list_free(tmplst);
         }

         /* Move to the list */
         scaling_drivers[pc++] = drv;
      }
      dir_list_free(policy_dir);
      last_update = time(NULL);
   }
   return scaling_drivers;
}

bool set_cpu_scaling_min_frequency(
   cpu_scaling_driver_t *driver,
   uint32_t min_freq)
{
   char fpath[PATH_MAX_LENGTH];
   char value[16];
   sprintf(fpath, CPU_POLICIES_DIR "policy%u/scaling_min_freq",
      driver->policy_id);
   sprintf(value, "%" PRIu32 "\n", min_freq);
   if (filestream_write_file(fpath, value, strlen(value)))
   {
      driver->min_policy_freq = min_freq;
      last_update = 0;   /* Force reload */
      return true;
   }
   return false;
}

bool set_cpu_scaling_max_frequency(
   cpu_scaling_driver_t *driver,
   uint32_t max_freq)
{
   char fpath[PATH_MAX_LENGTH];
   char value[16];
   sprintf(fpath, CPU_POLICIES_DIR "policy%u/scaling_max_freq",
      driver->policy_id);
   sprintf(value, "%" PRIu32 "\n", max_freq);
   if (filestream_write_file(fpath, value, strlen(value)))
   {
      driver->max_policy_freq = max_freq;
      last_update = 0;   /* Force reload */
      return true;
   }
   return false;
}

uint32_t get_cpu_scaling_next_frequency(
   cpu_scaling_driver_t *driver,
   uint32_t freq,
   int step)
{
   /* If the driver does not have a list of available frequencies */
   if (driver->available_freqs)
   {
      uint32_t *fr = driver->available_freqs;
      while (*fr)
      {
         if (fr[0] <= freq && fr[1] > freq && step > 0)
         {
            freq = fr[1];
            break;
         }
         else if (fr[0] < freq && fr[1] >= freq && step < 0)
         {
            freq = fr[0];
            break;
         }
         fr++;
      }
      if (!(*fr))
      {
         if (step > 0)
            freq = driver->max_cpu_freq;
         else
            freq = driver->min_cpu_freq;
      }
   }
   else {
      /* Just do small steps towards the max/min, arbitrary 100MHz */
      freq = freq + step * 100000;
   }

   if (freq > driver->max_cpu_freq)
      freq = driver->max_cpu_freq;
   if (freq < driver->min_cpu_freq)
      freq = driver->min_cpu_freq;

   return freq;
}

bool set_cpu_scaling_governor(cpu_scaling_driver_t *driver, const char* governor)
{
   char fpath[PATH_MAX_LENGTH];
   sprintf(fpath, CPU_POLICIES_DIR "policy%u/scaling_governor",
      driver->policy_id);
   if (filestream_write_file(fpath, governor, strlen(governor)))
   {
      if (driver->scaling_governor)
         free(driver->scaling_governor);
      driver->scaling_governor = strdup(governor);
      last_update = 0;   /* Force reload */
      return true;
   }
   return false;
}



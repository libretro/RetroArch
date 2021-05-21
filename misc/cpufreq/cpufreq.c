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
#include "../../configuration.h"

#define REFRESH_TIMEOUT  2
#define CPU_POLICIES_DIR "/sys/devices/system/cpu/cpufreq/"

static time_t last_update = 0;
static cpu_scaling_driver_t **scaling_drivers = NULL;
/* Mode state and its options */
static enum cpu_scaling_mode cur_smode = CPUSCALING_MANAGED_PERFORMANCE;
static cpu_scaling_opts_t cur_smode_opts = { 1, ~0U, "performance", "ondemand" };
/* Precalculate and store the absolute max and min frequencies */
static uint32_t abs_min_freq = 1, abs_max_freq = ~0U;

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

         /* Check current freq limits and update them */
         if (abs_min_freq > drv->min_cpu_freq || abs_min_freq == 1)
            abs_min_freq = drv->min_cpu_freq;
         if (abs_max_freq < drv->max_cpu_freq || abs_max_freq == ~0U)
            abs_max_freq = drv->max_cpu_freq;

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

         /* This is not available in many platforms! */
         fill_pathname_join(fpath, policy_dir->elems[i].data,
            "scaling_available_frequencies", sizeof(fpath));
         tmplst = readparse_list(fpath);
         if (tmplst)
         {
            drv->available_freqs = calloc(tmplst->size, sizeof(uint32_t));
            for (j = 0; j < tmplst->size; j++)
            {
               uint32_t freq = (uint32_t)atol(tmplst->elems[j].data);
               drv->available_freqs[j] = freq;
               if (abs_min_freq > freq || abs_min_freq == 1)
                  abs_min_freq = freq;
               if (abs_max_freq < freq || abs_max_freq == ~0U)
                  abs_max_freq = freq;
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
   snprintf(fpath, sizeof(fpath), CPU_POLICIES_DIR "policy%u/scaling_min_freq",
      driver->policy_id);
   snprintf(value, sizeof(value), "%" PRIu32 "\n", min_freq);
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
   snprintf(fpath, sizeof(fpath), CPU_POLICIES_DIR "policy%u/scaling_max_freq",
      driver->policy_id);
   snprintf(value, sizeof(value), "%" PRIu32 "\n", max_freq);
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

   freq = MIN(freq, driver->max_cpu_freq);
   freq = MAX(freq, driver->min_cpu_freq);

   return freq;
}

uint32_t get_cpu_scaling_next_frequency_limit(uint32_t freq, int step)
{
   /* Tune step, if it's smaller than 100MHz */
   unsigned fstep = 100000;
   if ((abs_max_freq - abs_min_freq) / 20 < fstep)
      fstep = 50000;

   if (freq <= abs_min_freq && step < 0)
      return 1;   /* Means "minimum frequency" */

   if (freq >= abs_max_freq && step > 0)
      return ~0U;   /* Means "maximum frequency" */

   /* Just do small steps towards the max/min */
   freq = freq + step * fstep;

   freq = MIN(freq, abs_max_freq);
   freq = MAX(freq, abs_min_freq);

   return freq;
}

bool set_cpu_scaling_governor(cpu_scaling_driver_t *driver, const char* governor)
{
   char fpath[PATH_MAX_LENGTH];
   snprintf(fpath, sizeof(fpath), CPU_POLICIES_DIR "policy%u/scaling_governor",
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

static void steer_all_drivers(
   const char *governor,
   uint32_t minfreq,
   uint32_t maxfreq)
{
   cpu_scaling_driver_t **drivers = get_cpu_scaling_drivers(false);
   if (!drivers)
      return;
   while (*drivers)
   {
      cpu_scaling_driver_t *d = *drivers++;
      if (minfreq)
         set_cpu_scaling_min_frequency(d, MAX(minfreq, d->min_cpu_freq));
      if (maxfreq)
         set_cpu_scaling_max_frequency(d, MIN(maxfreq, d->max_cpu_freq));
      set_cpu_scaling_governor(d, governor);
   }
}

void set_cpu_scaling_signal(enum cpu_scaling_event event)
{
   switch (cur_smode) {
   case CPUSCALING_MANAGED_PERFORMANCE:
      /* Bump to perf or fall back to ondemand depending on the RA state */
      if (event == CPUSCALING_EVENT_FOCUS_CORE)
         steer_all_drivers("performance", cur_smode_opts.min_freq,
            cur_smode_opts.max_freq);
      else
         steer_all_drivers("ondemand", 1, ~0U);
      break;
   case CPUSCALING_MANAGED_PER_CONTEXT:
      /* Apply the right settings the user specified */
      if (event == CPUSCALING_EVENT_FOCUS_CORE)
         steer_all_drivers(cur_smode_opts.main_policy, cur_smode_opts.min_freq,
            cur_smode_opts.max_freq);
      else
         steer_all_drivers(cur_smode_opts.menu_policy, 1, ~0U);
      break;
   default:
      break;
   };
}

enum cpu_scaling_mode get_cpu_scaling_mode(cpu_scaling_opts_t *opts)
{
   if (opts)
      *opts = cur_smode_opts;
   return cur_smode;
}

void set_cpu_scaling_mode(
   enum cpu_scaling_mode mode,
   const cpu_scaling_opts_t *opts)
{
   settings_t *settings = config_get_ptr();

   /* Store current state */
   cur_smode = mode;
   if (opts)
      cur_smode_opts = *opts;

   switch (mode)
   {
   case CPUSCALING_MANUAL:
      /* Do nothing, the UI allows for tweaking directly */
      break;
   case CPUSCALING_MANAGED_PERFORMANCE:
   case CPUSCALING_MANAGED_PER_CONTEXT:
      /* Simulate a state change to enforce the policy */
      set_cpu_scaling_signal(CPUSCALING_EVENT_FOCUS_MENU);
      break;
   case CPUSCALING_MAX_PERFORMANCE:
      // Set performance and bump frequencies to min/max
      steer_all_drivers("performance", 1, ~0U);
      break;
   case CPUSCALING_MIN_POWER:
      // Set powersave and bump frequencies to min/max
      steer_all_drivers("powersave", 1, ~0U);
      break;
   case CPUSCALING_BALANCED:
      // Set ondemand and bump frequencies to min/max
      steer_all_drivers("ondemand", 1, ~0U);
      break;
   };

   if (settings)
   {
      /* Store current settings */
      settings->uints.cpu_scaling_mode = (int)cur_smode;
      settings->uints.cpu_min_freq = cur_smode_opts.min_freq;
      settings->uints.cpu_max_freq = cur_smode_opts.max_freq;

      strlcpy(settings->arrays.cpu_main_gov, cur_smode_opts.main_policy,
         sizeof(settings->arrays.cpu_main_gov));
      strlcpy(settings->arrays.cpu_menu_gov, cur_smode_opts.menu_policy,
         sizeof(settings->arrays.cpu_menu_gov));
   }
};

void cpu_scaling_driver_free()
{
   if (scaling_drivers)
      free_drivers(scaling_drivers);

   scaling_drivers = NULL;
   last_update = 0;
}

void cpu_scaling_driver_init(void)
{
   /* Read the default settings */
   settings_t *settings = config_get_ptr();
   unsigned mode = settings->uints.cpu_scaling_mode;
   cur_smode_opts.min_freq = settings->uints.cpu_min_freq;
   cur_smode_opts.max_freq = settings->uints.cpu_max_freq;

   if (mode <= (int)CPUSCALING_MANUAL)
      cur_smode = (enum cpu_scaling_mode)mode;

   if (settings->arrays.cpu_main_gov[0])
      strlcpy(cur_smode_opts.main_policy, settings->arrays.cpu_main_gov,
         sizeof(cur_smode_opts.main_policy));
   if (settings->arrays.cpu_menu_gov[0])
      strlcpy(cur_smode_opts.menu_policy, settings->arrays.cpu_menu_gov,
         sizeof(cur_smode_opts.menu_policy));

   /* Force update the policy tree */
   get_cpu_scaling_drivers(true);

   /* Force enforce these settings */
   set_cpu_scaling_mode(cur_smode, NULL);
}


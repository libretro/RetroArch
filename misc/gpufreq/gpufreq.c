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

#include "gpufreq.h"
#include "../../configuration.h"

#define GPU_DEBUG 1

#define REFRESH_TIMEOUT  2
#ifdef HAVE_LAKKA_SWITCH
#define GPU_POLICIES_DIR "/sys/devices/gpu.0/devfreq/57000000.gpu/"
/*
else
 * #define GPU_POLICIES_DIR "Proper/path/for/board/" // WIP  
*/
#endif
/*
 * Available Switch gpu governors.
 * wmark_active nvhost_podgov(Default) userspace simple_ondemand
 */

static time_t last_update = 0;
static gpu_scaling_driver_t **scaling_drivers = NULL;
/* Mode state and its options */
static enum gpu_scaling_mode cur_smode = GPUSCALING_MANAGED_PERFORMANCE;
#ifdef HAVE_LAKKA_SWITCH
static gpu_scaling_opts_t cur_smode_opts = { 1, ~0U };
/*

#else
 * static gpu_scaling_opts_t cur_smode_opts = { 1, ~0U, "nvhost_podgov", "simple_ondemand" };
 * //needs to be set properly for each GPU that we decide to add support for. */
#endif

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

static void free_drivers(gpu_scaling_driver_t **d)
{
   if (d)
   {
      gpu_scaling_driver_t **it = d;
      while (*it)
      {
         gpu_scaling_driver_t *drv = *it++;
         if (drv->available_freqs)
            free(drv->available_freqs);
         free(drv);
      }
      free(d);
   }
}

gpu_scaling_driver_t **get_gpu_scaling_drivers(bool can_update)
{
   if (can_update && (time(NULL) > last_update + REFRESH_TIMEOUT ||
       !scaling_drivers))
   {
      /* Parse /sys/devices/system/cpu/cpufreq/ policies */
      int i, j, pc;
      struct string_list *policy_dir = dir_list_new(GPU_POLICIES_DIR, NULL,
        true, false, false, false);
      if (!policy_dir)
         return NULL;
      dir_list_sort(policy_dir, false);

      /* Delete the previous list of drivers */
      free_drivers(scaling_drivers);

      scaling_drivers = (gpu_scaling_driver_t**)calloc(
         (policy_dir->size + 1), sizeof(gpu_scaling_driver_t*));
      for (i = 0, pc = 0; i < policy_dir->size; i++)
      {
         uint32_t polid;
         gpu_scaling_driver_t *drv;
         struct string_list *tmplst;
         char fpath[PATH_MAX_LENGTH];
         const char *fname = strrchr(policy_dir->elems[i].data, '/');

         if (!fname)
            continue;

         drv = calloc(1, sizeof(gpu_scaling_driver_t));
         drv->policy_id = pc;

         /* Read all nodes with freq info */
         fill_pathname_join(fpath, GPU_POLICIES_DIR,
            "cur_freq", sizeof(fpath));
         readparse_uint32(fpath, &drv->current_frequency);

         fill_pathname_join(fpath, GPU_POLICIES_DIR,
            "max_freq", sizeof(fpath));
         readparse_uint32(fpath, &drv->max_policy_freq);

         /* This is not available in many platforms! */
         fill_pathname_join(fpath, /*policy_dir->elems[i].data*/ GPU_POLICIES_DIR,
            "available_frequencies", sizeof(fpath));
         tmplst = readparse_list(fpath);
         if (tmplst)
         {
            drv->available_freqs = calloc(tmplst->size + 1, sizeof(uint32_t));
            int freq_count = 0;
            for (j = 0; j < tmplst->size; j++)
            {
               uint32_t freq = (uint32_t)atol(tmplst->elems[j].data);
               drv->available_freqs[j] = freq;
               #if 0//def GPU_DEBUG
               RARCH_LOG("[GPU_DEBUG] Available Freq (%i): %u\n", j, drv->available_freqs[j]);
               #endif
               if (abs_min_freq > freq || abs_min_freq == 1)
                  abs_min_freq = freq;
               if (abs_max_freq < freq || abs_max_freq == ~0U)
                  abs_max_freq = freq;
            }
            freq_count = tmplst->size;
            string_list_free(tmplst);
            int a, b;
            for (a =0; a < freq_count -1; a++)
            {
               for (b=0; b < freq_count - 1 - a; b++)
               {
                  if (drv->available_freqs[b] > drv->available_freqs[b + 1])
                  {
                    uint32_t tmp = drv->available_freqs[b];
                    drv->available_freqs[b] = drv->available_freqs[b + 1];
                    drv->available_freqs[b + 1 ] = tmp;
                  }
               }
            }
         }

         /* Move to the list */
         scaling_drivers[pc++] = drv;
      }
      dir_list_free(policy_dir);
      last_update = time(NULL);
   }
   return scaling_drivers;
}

bool set_gpu_scaling_min_frequency(
   gpu_scaling_driver_t *driver,
   uint32_t min_freq)
{
   char fpath[PATH_MAX_LENGTH];
   char value[16];
   snprintf(fpath, sizeof(fpath), GPU_POLICIES_DIR "min_freq");
   snprintf(value, sizeof(value), "%" PRIu32 "\n", min_freq);
   if (filestream_write_file(fpath, value, strlen(value)))
   {
      driver->min_policy_freq = min_freq;
      last_update = 0;   /* Force reload */
      return true;
   }
   return false;
}

bool set_gpu_scaling_max_frequency(
   gpu_scaling_driver_t *driver,
   uint32_t max_freq)
{
   char fpath[PATH_MAX_LENGTH];
   char value[16];
   snprintf(fpath, sizeof(fpath), GPU_POLICIES_DIR "max_freq");
   snprintf(value, sizeof(value), "%" PRIu32 "\n", max_freq);
   if (filestream_write_file(fpath, value, strlen(value)))
   {
      driver->max_policy_freq = max_freq;
      last_update = 0;   /* Force reload */
      return true;
   }
   return false;
}

uint32_t get_gpu_scaling_next_frequency(
   gpu_scaling_driver_t *driver,
   uint32_t freq,
   int step)
{
   /* If the driver does not have a list of available frequencies */
   if (driver->available_freqs)
   {

      if (freq <= driver->available_freqs[0] && step < 0)
         return 1;   /* Means "minimum frequency" */

      if (freq >= abs_max_freq && step > 0)
         return ~0U;   /* Means "maximum frequency" */

      if (freq < abs_min_freq && step > 0)
         freq = abs_min_freq;

      if (freq == ~0U && step < 0)
        freq = abs_max_freq;

      uint32_t *fr = driver->available_freqs;
      while (*fr)
      {
         if (fr[0]  <= freq && fr[1] > freq && step > 0)
         {
            if (fr[1] != abs_max_freq)
              freq = fr[1];
            else
              freq = ~0U;
            break;
         }
         else if (fr[0] < freq && fr[1] >= freq && step < 0)
         {
            if (fr[0] != abs_min_freq)
              freq = fr[0];
            else
              freq = 1;
            break;
         }
         fr++;
      }
      if (!(*fr))
      {
         if (step > 0)
            freq = abs_max_freq;
         else
            freq = abs_min_freq;
      }
  }
   else {
      /* Just do small steps towards the max/min, arbitrary 100MHz */
      freq = freq + step * 100000;
      freq = MIN(freq, abs_max_freq);
      freq = MAX(freq, abs_min_freq);
   }
   return freq;
}

uint32_t get_gpu_scaling_next_frequency_limit(uint32_t freq, int step)
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

static void steer_all_drivers(uint32_t minfreq, uint32_t maxfreq)
{
   gpu_scaling_driver_t **drivers = get_gpu_scaling_drivers(false);
   if (!drivers)
      return;
   while (*drivers)
   {
      gpu_scaling_driver_t *d = *drivers++;
      if (minfreq){
        if  (d->available_freqs){
         if (minfreq == 1)
           set_gpu_scaling_min_frequency(d, abs_min_freq);
         else{
           RARCH_LOG("[GPU_DEBUG]: Min Freq: %u\n", minfreq);
           set_gpu_scaling_min_frequency(d,minfreq);
         }
        }
        else
         set_gpu_scaling_min_frequency(d, MAX(minfreq, abs_min_freq));
      }
      if (maxfreq) {
        if (d->available_freqs) {
          if (maxfreq == ~0U)
            set_gpu_scaling_max_frequency(d, abs_max_freq);
          else{
            RARCH_LOG("[GPU_DEBUG]: Max Freq: %u\n", maxfreq);
            set_gpu_scaling_max_frequency(d, maxfreq);
          }
        }
        else
          set_gpu_scaling_max_frequency(d, MIN(maxfreq, abs_max_freq));
      }
   }
}

void set_gpu_scaling_signal(enum gpu_scaling_event event)
{
   switch (cur_smode) {
   case GPUSCALING_MANAGED_PERFORMANCE:
      /* Change clocks based upon retroarch state, minimum for menu, user defined for core */
      if (event == GPUSCALING_EVENT_FOCUS_CORE)
         steer_all_drivers(cur_smode_opts.min_freq,
            cur_smode_opts.max_freq);
      else
         steer_all_drivers(abs_min_freq, abs_min_freq);
      break;
   default:
      break;
   };
}

enum gpu_scaling_mode get_gpu_scaling_mode(gpu_scaling_opts_t *opts)
{
   if (opts)
      *opts = cur_smode_opts;
   return cur_smode;
}

void set_gpu_scaling_mode(
   enum gpu_scaling_mode mode,
   const gpu_scaling_opts_t *opts)
{
   settings_t *settings = config_get_ptr();

   /* Store current state */
   cur_smode = mode;
   if (opts)
      cur_smode_opts = *opts;

   switch (mode)
   {
   case GPUSCALING_MANAGED_PERFORMANCE:
      /* Simulate a state change to enforce the policy */
      set_gpu_scaling_signal(GPUSCALING_EVENT_FOCUS_MENU);
      break;
   case GPUSCALING_MAX_PERFORMANCE:
      // bump frequencies to max/max
      steer_all_drivers(abs_max_freq, abs_max_freq);
      break;
   case GPUSCALING_MIN_POWER:
      // Set frequencies to min/min
      steer_all_drivers(abs_min_freq, abs_min_freq);
      break;
   case GPUSCALING_BALANCED:
      // Set ondemand and bump frequencies to min/max
      steer_all_drivers(abs_min_freq, abs_max_freq);
      break;
   };

   if (settings)
   {
      /* Store current settings */
      settings->uints.gpu_scaling_mode = (int)cur_smode;
      settings->uints.gpu_min_freq = cur_smode_opts.min_freq;
      settings->uints.gpu_max_freq = cur_smode_opts.max_freq;
   }
};

void gpu_scaling_driver_free()
{
   if (scaling_drivers)
      free_drivers(scaling_drivers);

   scaling_drivers = NULL;
   last_update = 0;
}

void gpu_scaling_driver_init(void)
{
   /* Read the default settings */
   settings_t *settings = config_get_ptr();
   unsigned mode = settings->uints.gpu_scaling_mode;
   gpu_scaling_opts_t loaded_opts;

   loaded_opts.min_freq = settings->uints.gpu_min_freq;
   loaded_opts.max_freq = settings->uints.gpu_max_freq;

   cur_smode_opts.min_freq = settings->uints.gpu_min_freq;
   cur_smode_opts.max_freq = settings->uints.gpu_max_freq;

   if (mode <= (unsigned)GPUSCALING_BALANCED)
      cur_smode = (enum gpu_scaling_mode)mode;

   /* Force update the policy tree */
   get_gpu_scaling_drivers(true);

   /* Force enforce these settings */
   set_gpu_scaling_mode(cur_smode, &loaded_opts);
}


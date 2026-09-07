/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2021 - Chris Kennedy, Antonio Giner,
 *                            Alexandre Wodarczyk, Gil Delescluse
 *  Copyright (C) 2026 - The RetroArch team
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include "modeline_list.h"
#include "modeline_monitor.h"

#ifndef MODELINE_STANDALONE
#include "../../verbosity.h"
#endif

void modeline_log_mode(const video_modeline_ops_t *ops,
      const video_modeline_t *mode)
{
   char txt[256];
   RARCH_DBG("[Modeline] %s timing %s\n",
         (ops && ops->name) ? ops->name : "dummy",
         modeline_print(mode, txt, sizeof(txt), MODELINE_PRINT_FULL));
}

static void modeline_filter_modes(video_modeline_gen_t *gen)
{
   int i;
   unsigned caps = gen->caps;

   for (i = 0; i < gen->num_modes; i++)
   {
      video_modeline_t *mode = &gen->modes[i];

      if (gen->refresh_dont_care)
         mode->type |= MODELINE_V_FREQ_EDITABLE;

      if (caps & MODELINE_CAPS_UPDATE)
         mode->type |= MODELINE_V_FREQ_EDITABLE;

      if (caps & MODELINE_CAPS_SCAN_EDITABLE)
         mode->type |= MODELINE_SCAN_EDITABLE;

      if (!gen->modeline_generation)
         mode->type &= ~(MODELINE_XYV_EDITABLE | MODELINE_SCAN_EDITABLE);

      if ((mode->type & MODELINE_DESKTOP)
            && !(caps & MODELINE_CAPS_DESKTOP_EDITABLE))
         mode->type &= ~MODELINE_V_FREQ_EDITABLE;

      if (gen->lock_system_modes && (mode->type & MODELINE_TIMING_SYSTEM))
         mode->type |= MODELINE_DISABLED;

      /* The desktop mode stays unlocked as the fallback */
      if (mode->type & MODELINE_DESKTOP)
         mode->type &= ~MODELINE_DISABLED;

      /* Lock every mode that does not match the user's resolution rules */
      if (gen->user_mode.width != 0 || gen->user_mode.height != 0
            || gen->user_mode.refresh != 0)
      {
         if (!(   (mode->width == gen->user_mode.width
                     || (mode->type & MODELINE_X_RES_EDITABLE)
                     || gen->user_mode.width == 0)
               && (mode->height == gen->user_mode.height
                  || (mode->type & MODELINE_Y_RES_EDITABLE)
                  || gen->user_mode.height == 0)
               && (mode->refresh == gen->user_mode.refresh
                  || (mode->type & MODELINE_V_FREQ_EDITABLE)
                  || gen->user_mode.refresh == 0)))
            mode->type |= MODELINE_DISABLED;
         else
            mode->type &= ~MODELINE_DISABLED;
      }
   }
}

static void modeline_apply_user_mode(video_modeline_gen_t *gen,
      const video_modeline_t *mode)
{
   gen->user_mode = *mode;
   modeline_filter_modes(gen);
}

static double modeline_parse_aspect(const char *aspect)
{
   int num, den;
   if (sscanf(aspect, "%d:%d", &num, &den) == 2)
   {
      if (den == 0)
      {
         RARCH_ERR("[Modeline] Aspect denominator can't be zero\n");
         return MODELINE_STANDARD_CRT_ASPECT;
      }
      return ((double)num / (double)den);
   }

   RARCH_ERR("[Modeline] Aspect must be <num:den>\n");
   return MODELINE_STANDARD_CRT_ASPECT;
}

static void modeline_set_preset(video_modeline_gen_t *gen)
{
   int i;
   size_t len = strlen(gen->monitor);
   for (i = 0; i < (int)len; i++)
      gen->monitor[i] = (char)tolower((unsigned char)gen->monitor[i]);

   memset(gen->range, 0, sizeof(gen->range));

   if (!strcmp(gen->monitor, "custom"))
   {
      for (i = 0; i < MODELINE_MAX_RANGES; i++)
         modeline_monitor_fill_range(&gen->range[i], gen->crt_range[i]);
   }
   else if (!strcmp(gen->monitor, "lcd"))
      modeline_monitor_fill_lcd_range(&gen->range[0], gen->lcd_range);
   else if (modeline_monitor_set_preset(gen->monitor, gen->range) == 0)
      modeline_monitor_set_preset("generic_15", gen->range);
}

void modeline_set_monitor(video_modeline_gen_t *gen, const char *preset)
{
   strlcpy(gen->monitor, preset, sizeof(gen->monitor));
   modeline_set_preset(gen);
}

void modeline_set_user_mode(video_modeline_gen_t *gen, int width,
      int height, int refresh)
{
   video_modeline_t user_mode;
   memset(&user_mode, 0, sizeof(user_mode));
   user_mode.width   = width;
   user_mode.height  = height;
   user_mode.refresh = refresh;
   modeline_apply_user_mode(gen, &user_mode);
}

video_modeline_gen_t *modeline_gen_new(void)
{
   int i;
   video_modeline_gen_t *gen = (video_modeline_gen_t*)calloc(1, sizeof(*gen));
   if (!gen)
      return NULL;

   gen->modes  = (video_modeline_t*)calloc(MODELINE_MAX_MODES, sizeof(*gen->modes));
   gen->backup = (video_modeline_t*)calloc(MODELINE_MAX_MODES, sizeof(*gen->backup));
   if (!gen->modes || !gen->backup)
   {
      modeline_gen_free(gen);
      return NULL;
   }

   /* Range lines carry decimals; parse them the same way on every locale */
   setlocale(LC_NUMERIC, "C");

   /* Display defaults */
   strlcpy(gen->monitor, "generic_15", sizeof(gen->monitor));
   strlcpy(gen->user_modeline, "auto", sizeof(gen->user_modeline));
   strlcpy(gen->lcd_range, "auto", sizeof(gen->lcd_range));
   for (i = 0; i < MODELINE_MAX_RANGES; i++)
      strlcpy(gen->crt_range[i], "auto", sizeof(gen->crt_range[i]));
   strlcpy(gen->disp.screen, "auto", sizeof(gen->disp.screen));
   strlcpy(gen->disp.custom_timing, "auto", sizeof(gen->disp.custom_timing));
   gen->modeline_generation         = true;
   gen->disp.lock_unsupported_modes = true;
   gen->lock_system_modes           = true;
   gen->refresh_dont_care           = false;

   /* Generator defaults */
   gen->interlace            = 1;
   gen->doublescan           = 1;
   gen->pclock_min           = 0;
   gen->monitor_aspect       = MODELINE_STANDARD_CRT_ASPECT;
   gen->refresh_tolerance    = 2.0;
   gen->super_width          = 2560;
   gen->h_shift              = 0;
   gen->v_shift              = 0;
   gen->h_size               = 1.0;
   gen->v_shift_correct      = 0;
   gen->pixel_precision      = 1;
   gen->interlace_force_even = 0;
   gen->scale_proportional   = 1;
   gen->caps                 = MODELINE_CAPS_ADD;

   modeline_set_preset(gen);
   return gen;
}

void modeline_gen_free(video_modeline_gen_t *gen)
{
   if (!gen)
      return;
   free(gen->modes);
   free(gen->backup);
   free(gen);
}

void modeline_set_option(video_modeline_gen_t *gen, const char *key,
      const char *value)
{
   if (!strcmp(key, "monitor"))
      modeline_set_monitor(gen, value);
   else if (!strncmp(key, "crt_range", 9) && key[9] >= '0' && key[9] <= '9'
         && key[10] == '\0')
      strlcpy(gen->crt_range[key[9] - '0'], value,
            sizeof(gen->crt_range[0]));
   else if (!strcmp(key, "lcd_range"))
      strlcpy(gen->lcd_range, value, sizeof(gen->lcd_range));
   else if (!strcmp(key, "modeline"))
      strlcpy(gen->user_modeline, value, sizeof(gen->user_modeline));
   else if (!strcmp(key, "user_mode"))
   {
      video_modeline_t user_mode;
      memset(&user_mode, 0, sizeof(user_mode));
      if (strcmp(value, "auto"))
      {
         if (sscanf(value, "%dx%d@%d", &user_mode.width, &user_mode.height,
                  &user_mode.refresh) < 1)
         {
            RARCH_ERR("[Modeline] user_mode must be <w>x<h>@<r>\n");
            return;
         }
      }
      modeline_apply_user_mode(gen, &user_mode);
   }
   /* Display options */
   else if (!strcmp(key, "display"))
      strlcpy(gen->disp.screen, value, sizeof(gen->disp.screen));
   else if (!strcmp(key, "api"))
      strlcpy(gen->disp.api, value, sizeof(gen->disp.api));
   else if (!strcmp(key, "modeline_generation"))
      gen->modeline_generation = atoi(value) ? true : false;
   else if (!strcmp(key, "lock_unsupported_modes"))
      gen->disp.lock_unsupported_modes = atoi(value) ? true : false;
   else if (!strcmp(key, "lock_system_modes"))
      gen->lock_system_modes = atoi(value) ? true : false;
   else if (!strcmp(key, "refresh_dont_care"))
      gen->refresh_dont_care = atoi(value) ? true : false;
   else if (!strcmp(key, "keep_changes"))
      gen->disp.keep_changes = atoi(value) ? true : false;
   /* Generator options */
   else if (!strcmp(key, "interlace"))
      gen->interlace = atoi(value);
   else if (!strcmp(key, "doublescan"))
      gen->doublescan = atoi(value);
   else if (!strcmp(key, "dotclock_min"))
   {
      double pclock_min = 0.0;
      sscanf(value, "%lf", &pclock_min);
      gen->pclock_min = (uint64_t)(pclock_min * 1000000);
   }
   else if (!strcmp(key, "sync_refresh_tolerance"))
   {
      double refresh_tolerance = 0.0;
      sscanf(value, "%lf", &refresh_tolerance);
      gen->refresh_tolerance = refresh_tolerance;
   }
   else if (!strcmp(key, "super_width"))
   {
      int super_width = 0;
      sscanf(value, "%d", &super_width);
      gen->super_width = super_width;
   }
   else if (!strcmp(key, "aspect"))
      gen->monitor_aspect = modeline_parse_aspect(value);
   else if (!strcmp(key, "h_size"))
   {
      double h_size = 1.0;
      sscanf(value, "%lf", &h_size);
      gen->h_size = h_size;
   }
   else if (!strcmp(key, "h_shift"))
   {
      int h_shift = 0;
      sscanf(value, "%d", &h_shift);
      gen->h_shift = h_shift;
   }
   else if (!strcmp(key, "v_shift"))
   {
      int v_shift = 0;
      sscanf(value, "%d", &v_shift);
      gen->v_shift = v_shift;
   }
   else if (!strcmp(key, "v_shift_correct"))
      gen->v_shift_correct = atoi(value);
   else if (!strcmp(key, "pixel_precision"))
      gen->pixel_precision = atoi(value);
   else if (!strcmp(key, "interlace_force_even"))
      gen->interlace_force_even = atoi(value);
   else if (!strcmp(key, "scale_proportional"))
      gen->scale_proportional = atoi(value);
   /* Backend options */
   else if (!strcmp(key, "screen_compositing"))
      gen->disp.screen_compositing = atoi(value) ? true : false;
   else if (!strcmp(key, "screen_reordering"))
      gen->disp.screen_reordering = atoi(value) ? true : false;
   else if (!strcmp(key, "allow_hardware_refresh"))
      gen->disp.allow_hardware_refresh = atoi(value) ? true : false;
   else if (!strcmp(key, "custom_timing"))
      strlcpy(gen->disp.custom_timing, value, sizeof(gen->disp.custom_timing));
   /* Logging goes through RetroArch's own verbosity */
   else if (!strcmp(key, "verbose") || !strcmp(key, "verbosity"))
      ;
   else
      RARCH_ERR("[Modeline] Invalid option %s\n", key);
}

void modeline_parse_options(video_modeline_gen_t *gen)
{
   video_modeline_t user_mode;

   RARCH_DBG("[Modeline] Display[%d] options: monitor[%s] generation[%s]\n",
         gen->index, gen->monitor, gen->modeline_generation ? "on" : "off");

   /* user_mode as <w>x<h>@<r> */
   modeline_apply_user_mode(gen, &gen->user_mode);

   /* A user modeline overrides user_mode */
   memset(&user_mode, 0, sizeof(user_mode));
   if (gen->modeline_generation)
   {
      if (modeline_parse(gen->user_modeline, &user_mode))
      {
         memset(gen->range, 0, sizeof(gen->range));
         user_mode.type |= MODELINE_USER_DEF;
         modeline_apply_user_mode(gen, &user_mode);
      }
   }

   /* Monitor specs */
   if (user_mode.hactive)
   {
      modeline_to_monitor_range(gen->range, &user_mode);
      modeline_monitor_show_range(gen->range);
   }
   else
      modeline_set_preset(gen);
}

/* LCD: derive the range from the desktop timing and pin the
 * resolution to the panel's native one. */
static bool modeline_auto_specs(video_modeline_gen_t *gen)
{
   video_modeline_t user_mode;

   if (gen->desktop_mode.width == 0 || gen->desktop_mode.height == 0
         || gen->desktop_mode.refresh == 0)
   {
      RARCH_ERR("[Modeline] Invalid desktop mode %dx%d@%d\n",
            gen->desktop_mode.width, gen->desktop_mode.height,
            gen->desktop_mode.refresh);
      return false;
   }

   RARCH_DBG("[Modeline] Creating automatic specs for LCD based on %s\n",
         (gen->desktop_mode.type & MODELINE_TIMING_SYSTEM)
         ? "VESA GTF" : "current timings");

   if (!strcmp(gen->lcd_range, "auto"))
   {
      snprintf(gen->lcd_range, sizeof(gen->lcd_range), "%d-%d",
            gen->desktop_mode.refresh - 1, gen->desktop_mode.refresh + 1);
      modeline_monitor_fill_lcd_range(gen->range, gen->lcd_range);
   }

   if (gen->desktop_mode.type & MODELINE_TIMING_SYSTEM)
      modeline_vesa_gtf(&gen->desktop_mode);
   modeline_to_monitor_range(gen->range, &gen->desktop_mode);
   modeline_monitor_show_range(gen->range);

   memset(&user_mode, 0, sizeof(user_mode));
   user_mode.width   = gen->desktop_mode.width;
   user_mode.height  = gen->desktop_mode.height;
   user_mode.refresh = gen->desktop_mode.refresh;
   modeline_apply_user_mode(gen, &user_mode);

   return true;
}

bool modeline_list_init(video_modeline_gen_t *gen,
      const video_modeline_ops_t *ops)
{
   int i;

   gen->caps       = (ops && ops->caps) ? ops->caps(ops->data) : MODELINE_CAPS_ADD;
   gen->num_modes  = 0;
   gen->num_backup = 0;
   gen->selected   = NULL;
   gen->current    = NULL;
   memset(&gen->desktop_mode, 0, sizeof(gen->desktop_mode));
   memset(gen->modes, 0, MODELINE_MAX_MODES * sizeof(*gen->modes));

   if (ops && ops->enum_modes)
   {
      int n = ops->enum_modes(ops->data, gen->modes, MODELINE_MAX_MODES);
      if (n < 0)
         return false;
      gen->num_modes = n;
   }

   for (i = 0; i < gen->num_modes; i++)
   {
      video_modeline_t *mode = &gen->modes[i];
      if (mode->type & MODELINE_DESKTOP)
      {
         gen->desktop_mode = *mode;
         if (!gen->current)
            gen->current = mode;
         if (mode->type & MODELINE_ROTATED)
            gen->desktop_is_rotated = true;
      }
      RARCH_DBG("[Modeline] [%3d] %4dx%4d @%3d%s%s %s: ", i + 1,
            mode->width, mode->height, mode->refresh,
            mode->interlace ? "i" : "p",
            (mode->type & MODELINE_DESKTOP) ? "*" : "",
            (mode->type & MODELINE_ROTATED) ? "rot" : "");
      modeline_log_mode(ops, mode);
   }

   memcpy(gen->backup, gen->modes, gen->num_modes * sizeof(*gen->modes));
   gen->num_backup = gen->num_modes;

   if (!strcmp(gen->monitor, "lcd"))
      modeline_auto_specs(gen);
   modeline_filter_modes(gen);

   return true;
}

video_modeline_t *modeline_find_id(video_modeline_gen_t *gen, int id)
{
   int i;
   for (i = 0; i < gen->num_modes; i++)
      if (gen->modes[i].id == id)
         return &gen->modes[i];
   return NULL;
}

video_modeline_t *modeline_get(video_modeline_gen_t *gen,
      const video_modeline_ops_t *ops, int width, int height,
      double refresh, int flags)
{
   int i;
   video_modeline_t s_mode;
   video_modeline_t t_mode;
   video_modeline_t best_mode;
   char result[256];
   unsigned caps   = gen->caps;
   bool rotated    = (flags & MODELINE_REQ_ROTATED) ? true : false;
   bool interlaced = (flags & MODELINE_REQ_INTERLACED) ? true : false;
   bool dummy      = false;

   memset(&s_mode, 0, sizeof(s_mode));
   memset(&t_mode, 0, sizeof(t_mode));
   memset(&best_mode, 0, sizeof(best_mode));

   RARCH_LOG("[Modeline] Calculating best video mode for %dx%d@%.6f%s orientation: %s\n",
         width, height, refresh, interlaced ? "i" : "",
         rotated ? "rotated" : "normal");

   best_mode.result.weight |= MODELINE_R_OUT_OF_RANGE;
   gen->selected            = NULL;

   s_mode.interlace = interlaced;
   s_mode.vfreq     = refresh;
   s_mode.hactive   = width;
   s_mode.vactive   = height;

   if (rotated)
   {
      int tmp        = s_mode.hactive;
      s_mode.hactive = s_mode.vactive;
      s_mode.vactive = tmp;
      s_mode.type   |= MODELINE_ROTATED;
   }

   /* A full list makes room by dropping the oldest generated mode
    * that is not on the wire, so a long session keeps switching */
   if ((caps & MODELINE_CAPS_ADD) && gen->modeline_generation
         && gen->num_modes >= MODELINE_MAX_MODES)
   {
      for (i = gen->num_backup; i < gen->num_modes; i++)
      {
         if (&gen->modes[i] == gen->current)
            continue;
         gen->modes[i].type |= MODELINE_DELETE;
         modeline_flush(gen, ops);
         break;
      }
   }

   /* An editable slot for a new mode, when the backend can add one */
   if ((caps & MODELINE_CAPS_ADD) && gen->modeline_generation
         && gen->num_modes < MODELINE_MAX_MODES)
   {
      video_modeline_t *new_mode = &gen->modes[gen->num_modes];
      memset(new_mode, 0, sizeof(*new_mode));
      new_mode->type = MODELINE_XYV_EDITABLE | MODELINE_V_FREQ_EDITABLE
         | MODELINE_SCAN_EDITABLE | MODELINE_ADD
         | (gen->desktop_is_rotated ? MODELINE_ROTATED : MODELINE_OK);
      gen->num_modes++;
      dummy = true;
   }

   for (i = 0; i < gen->num_modes; i++)
   {
      int r;
      video_modeline_t *mode = &gen->modes[i];

      RARCH_DBG("[Modeline] %s%4d%sx%s%4d%s_%s%d=%.6fHz%s%s\n",
            (mode->type & MODELINE_X_RES_EDITABLE) ? "(" : "[", mode->width,
            (mode->type & MODELINE_X_RES_EDITABLE) ? ")" : "]",
            (mode->type & MODELINE_Y_RES_EDITABLE) ? "(" : "[", mode->height,
            (mode->type & MODELINE_Y_RES_EDITABLE) ? ")" : "]",
            (mode->type & MODELINE_V_FREQ_EDITABLE) ? "(" : "[", mode->refresh,
            mode->vfreq,
            (mode->type & MODELINE_V_FREQ_EDITABLE) ? ")" : "]",
            (mode->type & MODELINE_DISABLED) ? " - locked" : "");

      if (mode->type & MODELINE_DISABLED)
         continue;

      for (r = 0; r < MODELINE_MAX_RANGES; r++)
      {
         if (gen->range[r].hfreq_min == 0)
            continue;

         t_mode = *mode;

         /* Editable fields start from the source or the user values */
         if (t_mode.type & MODELINE_X_RES_EDITABLE)
            t_mode.hactive = gen->user_mode.width
               ? gen->user_mode.width : s_mode.hactive;

         if (t_mode.type & MODELINE_Y_RES_EDITABLE)
            t_mode.vactive = gen->user_mode.height
               ? gen->user_mode.height : s_mode.vactive;

         if (t_mode.type & MODELINE_V_FREQ_EDITABLE)
         {
            /* A user vfreq means a user modeline: force it */
            if (gen->user_mode.vfreq)
               modeline_copy_timings(&t_mode, &gen->user_mode);
            else
               t_mode.vfreq = s_mode.vfreq;
         }

         if (gen->user_mode.width)
            t_mode.type &= ~MODELINE_X_RES_EDITABLE;
         if (gen->user_mode.height)
            t_mode.type &= ~MODELINE_Y_RES_EDITABLE;
         if (gen->user_mode.vfreq)
            t_mode.type &= ~MODELINE_V_FREQ_EDITABLE;

         modeline_create(&s_mode, &t_mode, &gen->range[r], gen);
         t_mode.range = r;

         RARCH_DBG("[Modeline]   %s\n",
               modeline_result(&t_mode, result, sizeof(result)));

         if (modeline_compare(&t_mode, &best_mode))
         {
            best_mode     = t_mode;
            gen->selected = mode;
         }
      }
   }

   /* The slot goes back unless it won */
   if (dummy && gen->selected != &gen->modes[gen->num_modes - 1])
      gen->num_modes--;

   if (best_mode.result.weight & MODELINE_R_OUT_OF_RANGE)
   {
      gen->selected = NULL;
      RARCH_ERR("[Modeline] Could not find a video mode that meets your specs\n");
      return NULL;
   }

   if ((best_mode.type & MODELINE_V_FREQ_EDITABLE)
         && !(best_mode.result.weight & MODELINE_R_OUT_OF_RANGE))
      modeline_adjust(&best_mode, gen->range[best_mode.range].hfreq_max, gen);

   RARCH_DBG("[Modeline] %s (%dx%d@%.6f)->(%dx%d@%.6f)\n",
         rotated ? "rotated" : "normal",
         width, height, refresh, best_mode.hactive, best_mode.vactive,
         best_mode.vfreq);
   RARCH_DBG("[Modeline]   %s\n",
         modeline_result(&best_mode, result, sizeof(result)));

   if (gen->modeline_generation)
   {
      if (best_mode.type & MODELINE_ADD)
      {
         best_mode.width   = best_mode.hactive;
         best_mode.height  = best_mode.vactive;
         best_mode.refresh = (int)best_mode.vfreq;
         /* A new mode is locked once generated */
         best_mode.type   &= ~(MODELINE_X_RES_EDITABLE | MODELINE_Y_RES_EDITABLE);
      }
      else if (modeline_is_different(&best_mode, gen->selected) != 0)
         best_mode.type |= MODELINE_UPDATE;

      RARCH_LOG("[Modeline] Modeline %s\n",
            modeline_print(&best_mode, result, sizeof(result), MODELINE_PRINT_FULL));
   }

   gen->switching_required = (gen->current != gen->selected
         || (best_mode.type & MODELINE_UPDATE)) ? true : false;

   if (best_mode.id == 0)
      best_mode.id = ++gen->id_counter;

   *gen->selected = best_mode;
   return gen->selected;
}

bool modeline_flush(video_modeline_gen_t *gen,
      const video_modeline_ops_t *ops)
{
   int i;
   bool error   = false;
   bool pending = false;

   for (i = 0; i < gen->num_modes; i++)
   {
      bool ok                = true;
      video_modeline_t *mode = &gen->modes[i];

      if (!(mode->type & (MODELINE_UPDATE | MODELINE_ADD | MODELINE_DELETE)))
         continue;
      pending = true;

      if (mode->type & MODELINE_DELETE)
      {
         if (ops && ops->del)
            ok = ops->del(ops->data, mode);
      }
      else if (mode->type & MODELINE_ADD)
      {
         if (ops && ops->add)
            ok = ops->add(ops->data, mode);
      }
      else if (mode->type & MODELINE_UPDATE)
      {
         if (ops && ops->update)
            ok = ops->update(ops->data, mode);
      }

      if (ok)
         mode->type &= ~MODELINE_ERROR;
      else
      {
         mode->type |= MODELINE_ERROR;
         error       = true;
      }

      RARCH_DBG("[Modeline] %s %s mode ",
            (mode->type & MODELINE_ERROR) ? "error" : "success",
            (mode->type & MODELINE_DELETE) ? "deleting"
            : (mode->type & MODELINE_ADD) ? "adding" : "updating");
      modeline_log_mode(ops, mode);
   }

   if (pending && ops && ops->flush)
   {
      if (!ops->flush(ops->data))
         error = true;
   }

   /* Reflect the changes in the list */
   for (i = gen->num_modes; i-- > 0; )
   {
      video_modeline_t *mode = &gen->modes[i];

      if (mode->type & MODELINE_ERROR)
         continue;

      if (mode->type & MODELINE_DELETE)
      {
         /* The pointers into the list follow the compaction */
         if (gen->current == mode)
            gen->current = NULL;
         else if (gen->current > mode)
            gen->current--;
         if (gen->selected == mode)
            gen->selected = NULL;
         else if (gen->selected > mode)
            gen->selected--;
         if (i + 1 < gen->num_modes)
            memmove(&gen->modes[i], &gen->modes[i + 1],
                  (gen->num_modes - i - 1) * sizeof(*gen->modes));
         gen->num_modes--;
      }
      else
         mode->type &= ~(MODELINE_UPDATE | MODELINE_ADD);
   }

   return !error;
}

bool modeline_restore(video_modeline_gen_t *gen,
      const video_modeline_ops_t *ops)
{
   int i;

   for (i = gen->num_modes; i-- > 0; )
   {
      /* Added modes go; modified ones return to their enumerated timing */
      if (i + 1 > gen->num_backup)
         gen->modes[i].type |= MODELINE_DELETE;
      else if (modeline_is_different(&gen->modes[i], &gen->backup[i]))
      {
         gen->modes[i]       = gen->backup[i];
         gen->modes[i].type |= MODELINE_UPDATE;
      }
   }
   return modeline_flush(gen, ops);
}

bool modeline_set(video_modeline_gen_t *gen,
      const video_modeline_ops_t *ops, video_modeline_t *mode)
{
   if (!mode)
      return false;

   if (gen->switching_required || gen->current != mode)
   {
      if (!ops || !ops->set || !ops->set(ops->data, mode))
      {
         RARCH_ERR("[Modeline] Error switching to %dx%d@%f\n",
               mode->hactive, mode->vactive, mode->vfreq);
         return false;
      }
      gen->current = mode;
      RARCH_LOG("[Modeline] Switched to %dx%d@%f\n",
            mode->hactive, mode->vactive, mode->vfreq);
   }
   else
      RARCH_LOG("[Modeline] Switching not required\n");

   return true;
}

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (turbo_bind_bounds_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Bounds test for the Turbo Bind / Turbo Button settings.
 *
 * Both settings hold a RetroPad ID that indexes fixed-size tables:
 * input_config_bind_order[24] from the menu's left/right handlers,
 * and settings->uints.input_remap_ids[port][] from the turbo paths
 * in input/input_driver.c.  The test drives every entry of both
 * settings left and right with Navigation Wrap-Around on and off,
 * feeds the handlers values from outside their range, runs the
 * load-time clamp over a table of config values, and runs the
 * turbo "clear underlying button" predicate with the empty bind.
 * The tables are heap- or global-allocated at their exact sizes so
 * AddressSanitizer reports any index past either end.
 *
 * The functions marked as verbatim copies follow
 * menu/menu_setting.c (setting_action_left_retropad_bind,
 * setting_action_right_retropad_bind), configuration.c
 * (config_sanitize_turbo_binds) and input/input_driver.c (the turbo
 * clear in input_state_device).  If those change, the copies here
 * must follow. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>

/* Production values from input/input_defines.h, libretro.h and
 * config.def.h. */
#define RARCH_FIRST_CUSTOM_BIND        16
#define RARCH_ANALOG_BIND_LIST_END     24
#define RARCH_CUSTOM_BIND_LIST_END     42
#define RETRO_DEVICE_ID_JOYPAD_B        0
#define RETRO_DEVICE_ID_JOYPAD_UP       4
#define DEFAULT_TURBO_BIND             -1
#define DEFAULT_TURBO_BUTTON           RETRO_DEVICE_ID_JOYPAD_B

#define SD_FLAG_ENFORCE_MINRANGE (1 << 0)
#define SD_FLAG_ENFORCE_MAXRANGE (1 << 1)

/* Copy of input_config_bind_order from input/input_driver.c. */
static const unsigned input_config_bind_order[24] = {
   4, 5, 6, 7, 0, 8, 1, 9, 2, 3, 10, 11, 12, 13, 14, 15,
   19, 18, 17, 16, 23, 22, 21, 20,
};

typedef struct
{
   struct { int *integer; } target;
} mock_value_t;

typedef struct
{
   mock_value_t value;
   float min;
   float max;
   unsigned flags;
} rarch_setting_t;

typedef struct
{
   struct { bool menu_navigation_wraparound_enable; } bools;
   struct { int input_turbo_bind; } ints;
   struct { unsigned input_turbo_button; } uints;
} settings_t;

static settings_t g_settings;
static settings_t *config_get_ptr(void) { return &g_settings; }

static int failures = 0;

/* === verbatim copy: menu/menu_setting.c === */
static int setting_action_left_retropad_bind(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int value       = 0;
   int max         = 0;
   int i           = 0;
   bool overflowed = false;

   if (!setting)
      return -1;

   value = *setting->value.target.integer;
   max   = (int)setting->max;

   /* input_config_bind_order holds every ID from 0 to max, so a
    * value outside that range (e.g. from a hand-edited config) is
    * not in it and is treated like the empty bind. */
   if (value < 0 || value > max)
      overflowed = true;
   else
   {
      for (i = 0; i < max; i++)
         if ((int)input_config_bind_order[i] == value)
            break;

      /* Left of the first entry is the empty bind, on the settings
       * whose range has one; the others stop or wrap below. */
      if (i > 0)
         *setting->value.target.integer = input_config_bind_order[i - 1];
      else if (setting->min < 0)
         *setting->value.target.integer = -1;
   }

   i--;

   if (setting->flags & SD_FLAG_ENFORCE_MINRANGE)
   {
      if (overflowed || i < setting->min)
      {
         settings_t *settings = config_get_ptr();

         if (settings &&
             settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.integer = input_config_bind_order[max];
      }
   }

   return 0;
}

static int setting_action_right_retropad_bind(
      rarch_setting_t *setting, size_t idx, bool wraparound)
{
   int value = 0;
   int max   = 0;
   int i     = 0;

   if (!setting)
      return -1;

   value = *setting->value.target.integer;
   max   = (int)setting->max;

   /* The empty bind and any value outside the range move to the
    * first entry. */
   if (value < 0 || value > max)
      *setting->value.target.integer = input_config_bind_order[0];
   else
   {
      for (i = 0; i < max; i++)
         if ((int)input_config_bind_order[i] == value)
            break;

      /* Right of the last entry stops there or wraps below. */
      if (i < max)
         *setting->value.target.integer = input_config_bind_order[i + 1];
   }

   i++;

   if (setting->flags & SD_FLAG_ENFORCE_MAXRANGE)
   {
      if (i > max)
      {
         settings_t *settings = config_get_ptr();
         int min              = (int)setting->min;
         if (settings && settings->bools.menu_navigation_wraparound_enable)
         {
            if (min < 0)
               *setting->value.target.integer = min;
            else
               *setting->value.target.integer = input_config_bind_order[min];
         }
      }
   }

   return 0;
}
/* === end verbatim copy === */

/* === verbatim copy: configuration.c === */
static void config_sanitize_turbo_binds(settings_t *settings)
{
   if (     settings->ints.input_turbo_bind < -1
         || settings->ints.input_turbo_bind >= RARCH_ANALOG_BIND_LIST_END)
      settings->ints.input_turbo_bind     = DEFAULT_TURBO_BIND;
   if (settings->uints.input_turbo_button >= RARCH_FIRST_CUSTOM_BIND)
      settings->uints.input_turbo_button  = DEFAULT_TURBO_BUTTON;
}
/* === end verbatim copy === */

/* === verbatim copy of the predicate in input/input_driver.c
 *     (input_state_device, the turbo "clear underlying button");
 *     remap_ids stands in for settings->uints.input_remap_ids. === */
static int turbo_clears(unsigned (*remap_ids)[RARCH_CUSTOM_BIND_LIST_END],
      unsigned port, int turbo_bind, unsigned id)
{
   if (     turbo_bind >= 0
         && id == remap_ids[port][turbo_bind])
      return 1;
   return 0;
}
/* === end verbatim copy === */

static void check(int cond, const char *what, int a, int b)
{
   if (!cond)
   {
      printf("[ERROR] %s (%d, %d)\n", what, a, b);
      failures++;
   }
}

/* A value the setting may hold: -1 where the range allows it, or a
 * RetroPad ID that is listed in its part of the bind order. */
static int is_valid(const rarch_setting_t *s, int v)
{
   int i;
   if (v == -1)
      return s->min < 0;
   for (i = 0; i <= (int)s->max; i++)
      if ((int)input_config_bind_order[i] == v)
         return 1;
   return 0;
}

static void walk(const char *name, int min, int max)
{
   int wrap;
   for (wrap = 0; wrap < 2; wrap++)
   {
      int v;
      int i;
      rarch_setting_t s;
      s.value.target.integer = &v;
      s.min   = (float)min;
      s.max   = (float)max;
      s.flags = SD_FLAG_ENFORCE_MINRANGE | SD_FLAG_ENFORCE_MAXRANGE;
      g_settings.bools.menu_navigation_wraparound_enable = wrap ? true : false;

      /* Right through every entry from the start of the range. */
      v = (min < 0) ? -1 : (int)input_config_bind_order[0];
      if (min < 0)
      {
         setting_action_right_retropad_bind(&s, 0, false);
         check(v == (int)input_config_bind_order[0], name, v, 0);
      }
      for (i = 1; i <= max; i++)
      {
         setting_action_right_retropad_bind(&s, 0, false);
         check(v == (int)input_config_bind_order[i], name, v, i);
      }
      /* Past the last entry: stop, or wrap to the start. */
      setting_action_right_retropad_bind(&s, 0, false);
      if (wrap)
         check(v == ((min < 0) ? -1 : (int)input_config_bind_order[0]),
               name, v, wrap);
      else
         check(v == (int)input_config_bind_order[max], name, v, wrap);

      /* Left through every entry from the end of the range. */
      v = (int)input_config_bind_order[max];
      for (i = max - 1; i >= 0; i--)
      {
         setting_action_left_retropad_bind(&s, 0, false);
         check(v == (int)input_config_bind_order[i], name, v, i);
      }
      /* Before the first entry: the empty bind where the range has
       * one, otherwise stop or wrap to the end. */
      setting_action_left_retropad_bind(&s, 0, false);
      if (min < 0)
         check(v == -1, name, v, wrap);
      else if (wrap)
         check(v == (int)input_config_bind_order[max], name, v, wrap);
      else
         check(v == (int)input_config_bind_order[0], name, v, wrap);
      check(is_valid(&s, v), name, v, wrap);
   }
}

static void out_of_range(const char *name, int min, int max)
{
   static const int bad[] = { -2, -1000, 24, 25, 99, INT_MAX, INT_MIN, 16, 19 };
   unsigned k;
   int wrap;
   for (wrap = 0; wrap < 2; wrap++)
   {
      g_settings.bools.menu_navigation_wraparound_enable = wrap ? true : false;
      for (k = 0; k < sizeof(bad) / sizeof(bad[0]); k++)
      {
         int v;
         rarch_setting_t s;
         s.value.target.integer = &v;
         s.min   = (float)min;
         s.max   = (float)max;
         s.flags = SD_FLAG_ENFORCE_MINRANGE | SD_FLAG_ENFORCE_MAXRANGE;

         /* 16 and 19 are in range for Turbo Bind; skip them there. */
         if (bad[k] >= 0 && bad[k] <= max)
            continue;

         v = bad[k];
         setting_action_right_retropad_bind(&s, 0, false);
         check(is_valid(&s, v), name, bad[k], v);

         v = bad[k];
         setting_action_left_retropad_bind(&s, 0, false);
         /* Without wrap-around Left leaves an unreadable value in
          * place; the load-time clamp keeps one from arriving. */
         if (wrap)
            check(is_valid(&s, v), name, bad[k], v);
      }
   }
}

static void sanitize(void)
{
   static const int binds[][2] = {
      { -1, -1 }, { 0, 0 }, { 23, 23 }, { 24, -1 }, { 9999, -1 },
      { -2, -1 }, { INT_MIN, -1 }, { INT_MAX, -1 },
   };
   static const unsigned buttons[][2] = {
      { 0, 0 }, { 15, 15 }, { 16, 0 }, { 23, 0 }, { UINT_MAX, 0 },
   };
   unsigned k;
   for (k = 0; k < sizeof(binds) / sizeof(binds[0]); k++)
   {
      settings_t s;
      memset(&s, 0, sizeof(s));
      s.ints.input_turbo_bind = binds[k][0];
      config_sanitize_turbo_binds(&s);
      check(s.ints.input_turbo_bind == binds[k][1], "sanitize bind",
            binds[k][0], s.ints.input_turbo_bind);
   }
   for (k = 0; k < sizeof(buttons) / sizeof(buttons[0]); k++)
   {
      settings_t s;
      memset(&s, 0, sizeof(s));
      s.uints.input_turbo_button = buttons[k][0];
      config_sanitize_turbo_binds(&s);
      check(s.uints.input_turbo_button == buttons[k][1], "sanitize button",
            (int)buttons[k][0], (int)s.uints.input_turbo_button);
   }
}

static void turbo_clear(void)
{
   /* Exactly one port's table, so remap_ids[0][-1] is reported. */
   unsigned (*remap_ids)[RARCH_CUSTOM_BIND_LIST_END] =
      (unsigned (*)[RARCH_CUSTOM_BIND_LIST_END])
      malloc(sizeof(unsigned) * RARCH_CUSTOM_BIND_LIST_END);
   unsigned id;
   int j;
   if (!remap_ids)
   {
      failures++;
      return;
   }
   for (j = 0; j < RARCH_CUSTOM_BIND_LIST_END; j++)
      remap_ids[0][j] = (unsigned)j;
   for (id = 0; id < RARCH_FIRST_CUSTOM_BIND; id++)
   {
      check(!turbo_clears(remap_ids, 0, -1, id), "empty bind clears", id, 0);
      check(turbo_clears(remap_ids, 0, (int)id, id) == 1,
            "bound button not cleared", id, 0);
   }
   free(remap_ids);
}

int main(void)
{
   walk("Turbo Bind walk", -1, RARCH_ANALOG_BIND_LIST_END - 1);
   walk("Turbo Button walk", 0, RARCH_FIRST_CUSTOM_BIND - 1);
   out_of_range("Turbo Bind range", -1, RARCH_ANALOG_BIND_LIST_END - 1);
   out_of_range("Turbo Button range", 0, RARCH_FIRST_CUSTOM_BIND - 1);
   sanitize();
   turbo_clear();

   if (failures)
   {
      printf("\n%d turbo_bind_bounds check(s) failed\n", failures);
      return 1;
   }
   printf("All turbo_bind_bounds checks passed.\n");
   return 0;
}

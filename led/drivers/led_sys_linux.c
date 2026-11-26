/*  RetroArch - A frontend for libretro.
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

/* sysfs LED class driver.
 *
 * ledN_map = M drives /sys/class/leds/ledM when that LED exists.
 * Otherwise M = 0, 1 and 2 drive every keyboard's Num Lock, Caps Lock
 * and Scroll Lock LED (*::numlock, *::capslock, *::scrolllock), the
 * same numbering the X11 and Win32 keyboard drivers use.
 *
 * An LED is taken over the first time a core sets it: its trigger,
 * brightness and max_brightness are read, then its trigger is set to
 * "none". Freeing the driver puts the brightness back, then the
 * trigger, so a keyboard LED follows its lock key again. */

#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include <stdint.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "../led_driver.h"
#include "../led_defines.h"

#include "../../configuration.h"

#ifndef SYSLED_ROOT
#define SYSLED_ROOT "/sys/class/leds"
#endif

/* One bit per target in a frontend LED's mask. */
#define SYSLED_MAX_TARGETS 32
#define SYSLED_NAME_LEN    64
#define SYSLED_TRIGGER_LEN 64
#define SYSLED_PATH_LEN    256

typedef struct
{
   char name[SYSLED_NAME_LEN];
   /* What was there before; restored on free. Empty when it could
    * not be read, and then left as "none". */
   char trigger[SYSLED_TRIGGER_LEN];
   int  brightness;
   int  max_brightness;
   int  state;          /* last brightness written, -1 for none */
} sysled_target_t;

typedef struct
{
   sysled_target_t target[SYSLED_MAX_TARGETS];
   uint32_t        leds[MAX_LEDS];  /* targets of each frontend LED */
   int             map[MAX_LEDS];
   uint32_t        resolved;        /* LEDs whose targets are known */
   unsigned        n_targets;
} sysled_t;

/* TODO/FIXME - static globals */
static sysled_t sys_curins;
static sysled_t *sys_cur = &sys_curins;

static const char *const sysled_keyboard_suffix[] = {
   "::numlock",
   "::capslock",
   "::scrolllock"
};

static int sysled_write(const char *name, const char *attr, const char *s)
{
   FILE *fp;
   int ok;
   char path[SYSLED_PATH_LEN];
   snprintf(path, sizeof(path), SYSLED_ROOT "/%s/%s", name, attr);
   if (!(fp = fopen(path, "w")))
      return 0;
   ok = (fputs(s, fp) >= 0);
   if (fclose(fp) != 0)
      ok = 0;
   return ok;
}

static int sysled_write_int(const char *name, const char *attr, int value)
{
   char buf[16];
   snprintf(buf, sizeof(buf), "%d\n", value);
   return sysled_write(name, attr, buf);
}

static int sysled_read_int(const char *name, const char *attr, int *value)
{
   FILE *fp;
   int ok;
   char path[SYSLED_PATH_LEN];
   snprintf(path, sizeof(path), SYSLED_ROOT "/%s/%s", name, attr);
   if (!(fp = fopen(path, "r")))
      return 0;
   ok = (fscanf(fp, "%d", value) == 1);
   fclose(fp);
   return ok;
}

/* The trigger file lists every trigger with the active one in
 * brackets, and can run to kilobytes; only the bracketed one is kept. */
static void sysled_read_trigger(const char *name, char *s, size_t len)
{
   FILE *fp;
   int c;
   size_t n = 0;
   char path[SYSLED_PATH_LEN];

   s[0] = '\0';
   snprintf(path, sizeof(path), SYSLED_ROOT "/%s/trigger", name);
   if (!(fp = fopen(path, "r")))
      return;

   while ((c = fgetc(fp)) != EOF && c != '[')
      ;
   if (c == '[')
   {
      while ((c = fgetc(fp)) != EOF && c != ']')
      {
         if (n + 1 >= len)
         {
            n = 0;
            break;
         }
         s[n++] = (char)c;
      }
      if (c != ']')
         n = 0;
   }
   s[n] = '\0';
   fclose(fp);
}

/* The target slot for the LED called name, taking it over on first
 * use; -1 when it cannot be driven. */
static int sysled_target(const char *name)
{
   unsigned i;
   sysled_target_t *t;

   for (i = 0; i < sys_cur->n_targets; i++)
      if (string_is_equal(sys_cur->target[i].name, name))
         return (int)i;

   if (     sys_cur->n_targets >= SYSLED_MAX_TARGETS
         || strlen(name) >= SYSLED_NAME_LEN)
      return -1;

   t = &sys_cur->target[sys_cur->n_targets];
   if (!sysled_read_int(name, "brightness", &t->brightness))
      return -1;
   if (     !sysled_read_int(name, "max_brightness", &t->max_brightness)
         || t->max_brightness <= 0)
      t->max_brightness = 1;
   sysled_read_trigger(name, t->trigger, sizeof(t->trigger));

   if (!sysled_write(name, "trigger", "none"))
      return -1;

   strlcpy(t->name, name, sizeof(t->name));
   t->state = -1;
   return (int)sys_cur->n_targets++;
}

static uint32_t sysled_resolve(int m)
{
   int slot;
   char name[SYSLED_NAME_LEN];
   uint32_t mask = 0;

   if (m < 0)
      return 0;

   snprintf(name, sizeof(name), "led%d", m);
   if ((slot = sysled_target(name)) >= 0)
      return (uint32_t)1 << slot;

   if (m < (int)ARRAY_SIZE(sysled_keyboard_suffix))
   {
      struct dirent *ent;
      DIR *dir           = opendir(SYSLED_ROOT);
      const char *suffix = sysled_keyboard_suffix[m];
      size_t suffix_len  = strlen(suffix);

      if (!dir)
         return 0;
      while ((ent = readdir(dir)))
      {
         size_t len = strlen(ent->d_name);
         if (     len > suffix_len
               && string_is_equal(ent->d_name + len - suffix_len, suffix)
               && (slot = sysled_target(ent->d_name)) >= 0)
            mask |= (uint32_t)1 << slot;
      }
      closedir(dir);
   }

   return mask;
}

static void sys_led_init(void)
{
   int i;
   settings_t *settings = config_get_ptr();

   memset(sys_cur, 0, sizeof(*sys_cur));

   if (!settings)
      return;

   for (i = 0; i < MAX_LEDS; i++)
      sys_cur->map[i] = (int)settings->uints.led_map[i];
}

static void sys_led_free(void)
{
   unsigned i;

   for (i = 0; i < sys_cur->n_targets; i++)
   {
      const sysled_target_t *t = &sys_cur->target[i];
      /* Brightness first: writing it after a trigger is set would
       * clear that trigger again. */
      sysled_write_int(t->name, "brightness", t->brightness);
      if (t->trigger[0] && !string_is_equal(t->trigger, "none"))
         sysled_write(t->name, "trigger", t->trigger);
   }

   memset(sys_cur, 0, sizeof(*sys_cur));
}

static void sys_led_set(int led, int state)
{
   unsigned i;
   uint32_t mask;

   /* Invalid LED? */
   if ((led < 0) || (led >= MAX_LEDS))
      return;

   if (!(sys_cur->resolved & ((uint32_t)1 << led)))
   {
      sys_cur->leds[led]  = sysled_resolve(sys_cur->map[led]);
      sys_cur->resolved  |= (uint32_t)1 << led;
   }

   for (mask = sys_cur->leds[led], i = 0; mask; mask >>= 1, i++)
   {
      sysled_target_t *t;
      int value;
      if (!(mask & 1))
         continue;
      t     = &sys_cur->target[i];
      value = state ? t->max_brightness : 0;
      if (t->state != value && sysled_write_int(t->name, "brightness", value))
         t->state = value;
   }
}

const led_driver_t sys_led_driver = {
   sys_led_init,
   sys_led_free,
   sys_led_set,
   "sysled"
};

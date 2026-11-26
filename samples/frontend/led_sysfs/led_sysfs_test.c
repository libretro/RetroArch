/* led_sysfs_test.c -- led/drivers/led_sys_linux.c against a fake
 * sysfs LED class directory.
 *
 * The driver is included with SYSLED_ROOT pointing at a directory this
 * test builds. The claims:
 *
 *   1. ledN_map = M drives ledM when it exists: the first set writes
 *      "none" to its trigger, on is max_brightness, off is 0.
 *   2. A state already written is not written again.
 *   3. With no ledM, M = 0, 1, 2 drive every *::numlock, *::capslock,
 *      *::scrolllock LED, and no other.
 *   4. The active trigger is taken from between the brackets, also at
 *      the end of a trigger file kilobytes long.
 *   5. Freeing puts each LED's brightness back, then its trigger.
 *   6. Two frontend LEDs mapped to one sysfs LED take it over once,
 *      so freeing restores what was there before either.
 *   7. An LED whose trigger cannot be written is left alone, and a
 *      negative map drives nothing.
 *   8. No LED is touched before a core sets one.
 *
 * sysfs LED names carry "::", which Windows file names cannot, so
 * claims 3, 4 and 6 run on the other hosts only. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef _WIN32
#include <direct.h>
#define test_mkdir(p) _mkdir(p)
#else
#include <unistd.h>
#define test_mkdir(p) mkdir(p, 0755)
#endif

#define SYSLED_ROOT "led_sysfs_root"

#include "configuration.h"

static settings_t test_settings;

settings_t *config_get_ptr(void) { return &test_settings; }

#include "led/drivers/led_sys_linux.c"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

static void rm_tree(const char *path)
{
   struct dirent *ent;
   DIR *dir = opendir(path);
   if (dir)
   {
      while ((ent = readdir(dir)))
      {
         char sub[512];
         if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;
         snprintf(sub, sizeof(sub), "%s/%s", path, ent->d_name);
         rm_tree(sub);
      }
      closedir(dir);
#ifdef _WIN32
      _rmdir(path);
#else
      rmdir(path);
#endif
   }
   else
      remove(path);
}

static void put(const char *led, const char *attr, const char *s)
{
   FILE *fp;
   char path[512];
   snprintf(path, sizeof(path), SYSLED_ROOT "/%s/%s", led, attr);
   if ((fp = fopen(path, "w")))
   {
      fputs(s, fp);
      fclose(fp);
   }
}

/* The file's content, newline trimmed. */
static const char *get(const char *led, const char *attr)
{
   static char buf[8192];
   FILE *fp;
   size_t n;
   char path[512];
   snprintf(path, sizeof(path), SYSLED_ROOT "/%s/%s", led, attr);
   buf[0] = '\0';
   if (!(fp = fopen(path, "r")))
      return buf;
   n      = fread(buf, 1, sizeof(buf) - 1, fp);
   buf[n] = '\0';
   fclose(fp);
   while (n && (buf[n - 1] == '\n'))
      buf[--n] = '\0';
   return buf;
}

static void make_led(const char *led, const char *trigger,
      const char *brightness, const char *max)
{
   char path[512];
   snprintf(path, sizeof(path), SYSLED_ROOT "/%s", led);
   test_mkdir(path);
   put(led, "trigger", trigger);
   put(led, "brightness", brightness);
   put(led, "max_brightness", max);
}

static void map_all(int value)
{
   int i;
   for (i = 0; i < MAX_LEDS; i++)
      test_settings.uints.led_map[i] = (unsigned)value;
}

static void test_numbered(void)
{
   char path[512];

   rm_tree(SYSLED_ROOT);
   test_mkdir(SYSLED_ROOT);
   make_led("led0", "none [mmc0] heartbeat\n", "1\n", "255\n");
   make_led("led1", "none [timer]\n", "0\n", "1\n");
   /* led2's trigger is a directory: not writable as a file. */
   make_led("led2", "none\n", "0\n", "1\n");
   snprintf(path, sizeof(path), SYSLED_ROOT "/led2/trigger");
   remove(path);
   test_mkdir(path);

   map_all(-1);
   test_settings.uints.led_map[0] = 0;
   test_settings.uints.led_map[1] = 0;   /* same LED as frontend LED 0 */
   test_settings.uints.led_map[2] = 2;
   test_settings.uints.led_map[3] = 1;

   sys_led_driver.init();

   CHECK(!strcmp(get("led0", "trigger"), "none [mmc0] heartbeat"),
         "init touched an LED");

   sys_led_driver.set_led(0, 1);
   CHECK(!strcmp(get("led0", "trigger"), "none"), "trigger not none");
   CHECK(!strcmp(get("led0", "brightness"), "255"),
         "on is not max_brightness");

   put("led0", "brightness", "x");
   sys_led_driver.set_led(0, 1);
   CHECK(!strcmp(get("led0", "brightness"), "x"),
         "unchanged state written again");
   sys_led_driver.set_led(1, 1);
   CHECK(!strcmp(get("led0", "brightness"), "x"),
         "second LED on the same target wrote again");

   sys_led_driver.set_led(1, 0);
   CHECK(!strcmp(get("led0", "brightness"), "0"), "off is not 0");
   CHECK(sys_cur->n_targets == 1, "shared LED taken over twice");

   sys_led_driver.set_led(2, 1);
   CHECK(!strcmp(get("led2", "brightness"), "0"),
         "LED with an unwritable trigger was driven");

   sys_led_driver.set_led(4, 1);   /* map -1 */
   CHECK(sys_cur->n_targets == 1, "negative map drove something");

   CHECK(!strcmp(get("led1", "trigger"), "none [timer]"),
         "unset LED touched");

   sys_led_driver.free();
   CHECK(!strcmp(get("led0", "brightness"), "1"), "brightness not restored");
   CHECK(!strcmp(get("led0", "trigger"), "mmc0"), "trigger not restored");
   CHECK(!strcmp(get("led1", "trigger"), "none [timer]"),
         "free touched an unset LED");
}

#ifndef _WIN32
static void test_keyboard(void)
{
   size_t n;
   static char longtrig[6000];

   /* Kilobytes of triggers ahead of the active one. */
   longtrig[0] = '\0';
   for (n = 0; strlen(longtrig) < 5000; n++)
   {
      char t[32];
      snprintf(t, sizeof(t), "kbd-trigger%u ", (unsigned)n);
      strcat(longtrig, t);
   }
   strcat(longtrig, "[kbd-capslock] kbd-numlock\n");

   rm_tree(SYSLED_ROOT);
   test_mkdir(SYSLED_ROOT);
   make_led("input3::capslock", longtrig, "1\n", "1\n");
   make_led("input7::capslock", "none [kbd-capslock]\n", "0\n", "1\n");
   make_led("input3::numlock", "none [kbd-numlock]\n", "0\n", "1\n");

   map_all(-1);
   test_settings.uints.led_map[0] = 1;   /* no led1: Caps Lock */
   test_settings.uints.led_map[1] = 1;

   sys_led_driver.init();
   sys_led_driver.set_led(0, 1);
   sys_led_driver.set_led(1, 1);
   CHECK(sys_cur->n_targets == 2, "not both Caps Lock LEDs, once each");
   CHECK(!strcmp(get("input3::capslock", "trigger"), "none"),
         "keyboard 1 trigger not none");
   CHECK(!strcmp(get("input7::capslock", "trigger"), "none"),
         "keyboard 2 trigger not none");
   CHECK(!strcmp(get("input7::capslock", "brightness"), "1"),
         "keyboard 2 not lit");
   CHECK(!strcmp(get("input3::numlock", "trigger"), "none [kbd-numlock]"),
         "Num Lock driven for a Caps Lock map");

   sys_led_driver.set_led(0, 0);
   CHECK(!strcmp(get("input3::capslock", "brightness"), "0"),
         "keyboard 1 not off");

   sys_led_driver.free();
   CHECK(!strcmp(get("input3::capslock", "trigger"), "kbd-capslock"),
         "long trigger file not restored");
   CHECK(!strcmp(get("input3::capslock", "brightness"), "1"),
         "keyboard 1 brightness not restored");
   CHECK(!strcmp(get("input7::capslock", "trigger"), "kbd-capslock"),
         "keyboard 2 trigger not restored");
   CHECK(!strcmp(get("input7::capslock", "brightness"), "0"),
         "keyboard 2 brightness not restored");
}
#endif

int main(void)
{
   test_numbered();
#ifndef _WIN32
   test_keyboard();
#endif
   rm_tree(SYSLED_ROOT);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("led_sysfs_test: all passed\n");
   return 0;
}

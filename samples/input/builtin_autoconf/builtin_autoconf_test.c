/* The built-in profiles of input/input_autodetect_builtin.c, which a
 * pad with no profile file gets, as a platform's build compiles them
 * in: the SDL2 and SDL3 drivers' in every build, and with -DWEBOS,
 * -DANDROID, -D__PS3__, -DORBIS, -DGEKKO or -D__QNX__ (against the
 * tree's BlackBerry 10 SDK stand-ins) that platform's too. Each
 * driver reads both of a pad's sticks the same way, so each right-stick
 * bind has the sign of its left-stick twin. Where the driver hands on
 * axes that are negative up and left, as SDL's gamepad axes are, down
 * is the + half too. Every build's profiles lead with the identity
 * tasks/task_autodetect.c ranks them by before it parses one. */
#include <stdio.h>
#include <string.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

extern const char* const input_builtin_autoconfs[];

static const char *const dirs[] = {
   "x_plus", "x_minus", "y_plus", "y_minus" };

/* drivers whose pads all have a right stick */
static const char *const two_stick_drivers[] = {
   "sdl2", "sdl3", "android", "ps3", "ps4" };

static const char *const up_negative_drivers[] = {
   "sdl2", "sdl3", "android", "ps3", "ps4", "gx" };

/* the profiles each build must have, by driver */
static const struct { const char *driver; unsigned want; } builds[] = {
#ifdef WEBOS
   { "sdl2", 4 },
#else
   { "sdl2", 1 },
#endif
   { "sdl3", 2 },
#if defined(ANDROID)
   { "android", 2 },
#elif defined(__PS3__)
   { "ps3", 1 },
#elif defined(ORBIS)
   { "ps4", 1 },
#elif defined(GEKKO) && defined(HW_RVL)
   { "gx", 5 },
#elif defined(GEKKO)
   { "gx", 1 },
#elif defined(__QNX__)
   { "qnx", 1 },
#endif
};

/* the sign written for input_<stick>_<dir>_axis, or 0 if unbound */
static char axis_sign(const char *conf, char stick, const char *dir)
{
   char key[64];
   const char *s;
   sprintf(key, "input_%c_%s_axis = ", stick, dir);
   if (!(s = strstr(conf, key)))
      return 0;
   s += strlen(key);
   return (*s == '+' || *s == '-') ? *s : 0;
}

static int has_driver(const char *conf, const char *driver)
{
   char key[64];
   sprintf(key, "input_driver = \"%s\"\n", driver);
   return strstr(conf, key) != NULL;
}

static int has_any_driver(const char *conf,
      const char *const *drivers, unsigned count)
{
   unsigned i;
   for (i = 0; i < count; i++)
      if (has_driver(conf, drivers[i]))
         return 1;
   return 0;
}

static int up_negative(const char *conf)
{
   /* which way up the XPERIA Play's touch pads read is unknown */
   if (strstr(conf, "input_device = \"XPERIA Play\"\n"))
      return 0;
   return has_any_driver(conf, up_negative_drivers,
         ARRAY_LEN(up_negative_drivers));
}

/* tasks/task_autodetect.c ranks a built-in from its leading lines
 * alone: the device name, or the product and vendor ids, and nothing
 * else that names a pad */
static int check_identity(const char *conf)
{
   static const char dev_key[] = "input_device = \"";
   static const char pid_key[] = "input_product_id = ";
   int name_len = (int)strcspn(conf, "\n");
   const char *nl;

   if (!strncmp(conf, dev_key, sizeof(dev_key) - 1))
   {
      const char *q = strchr(conf + sizeof(dev_key) - 1, '"');
      if (!q || q == conf + sizeof(dev_key) - 1 || q[1] != '\n')
      {
         printf("FAIL %.*s: device name is not one quoted line\n",
               name_len, conf);
         return 1;
      }
      if (     strstr(conf, "input_vendor_id")
            || strstr(conf, "input_product_id"))
      {
         printf("FAIL %.*s: a device name with ids\n", name_len, conf);
         return 1;
      }
   }
   else if (!strncmp(conf, pid_key, sizeof(pid_key) - 1))
   {
      nl = strchr(conf, '\n');
      if (!nl || strncmp(nl + 1, "input_vendor_id = ", 18))
      {
         printf("FAIL %.*s: input_vendor_id does not follow\n",
               name_len, conf);
         return 1;
      }
   }
   else
   {
      printf("FAIL %.*s: neither input_device nor input_product_id leads\n",
            name_len, conf);
      return 1;
   }

   for (nl = conf; (nl = strstr(nl, "_alt")); nl += 4)
      if (nl[4] >= '0' && nl[4] <= '9')
         break;
   if (     nl
         || strstr(conf + name_len, "\ninput_device = ")
         || strstr(conf, "input_phys"))
   {
      printf("FAIL %.*s: a second device name, an alternative or a physical location\n",
            name_len, conf);
      return 1;
   }
   return 0;
}

static int check_profile(const char *conf)
{
   int bad = 0;
   unsigned i;
   int name_len = (int)strcspn(conf, "\n");
   int sticks   = has_any_driver(conf, two_stick_drivers,
         ARRAY_LEN(two_stick_drivers));
   char down    = axis_sign(conf, 'l', "y_plus");

   for (i = 0; i < ARRAY_LEN(dirs); i++)
   {
      char l = axis_sign(conf, 'l', dirs[i]);
      char r = axis_sign(conf, 'r', dirs[i]);
      if ((r || sticks) && (!r || l != r))
      {
         printf("FAIL %.*s: r_%s_axis is %c, l_%s_axis is %c\n",
               name_len, conf, dirs[i], r ? r : '?',
               dirs[i], l ? l : '?');
         bad = 1;
      }
   }
   if (down && down != '+' && up_negative(conf))
   {
      printf("FAIL %.*s: l_y_plus_axis is not +\n", name_len, conf);
      bad = 1;
   }
   return bad;
}

int main(void)
{
   unsigned i, j;
   unsigned found[ARRAY_LEN(builds)] = {0};
   int failed = 0;

   for (i = 0; input_builtin_autoconfs[i]; i++)
   {
      const char *conf = input_builtin_autoconfs[i];
      for (j = 0; j < ARRAY_LEN(builds); j++)
         if (has_driver(conf, builds[j].driver))
            found[j]++;
      failed |= check_identity(conf);
      failed |= check_profile(conf);
   }

   for (j = 0; j < ARRAY_LEN(builds); j++)
   {
      if (found[j] < builds[j].want)
      {
         printf("FAIL %u %s profiles built in, want %u\n",
               found[j], builds[j].driver, builds[j].want);
         failed = 1;
      }
   }

   if (failed)
      return 1;
   printf("PASS");
   for (j = 0; j < ARRAY_LEN(builds); j++)
      printf(" %s %u", builds[j].driver, found[j]);
   printf("\n");
   return 0;
}

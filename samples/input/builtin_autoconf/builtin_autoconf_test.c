/* The built-in profiles of the SDL2 and SDL3 joypad drivers
 * (input/input_autodetect_builtin.c), which a pad with no profile file
 * gets. Those drivers read SDL's gamepad axes, where both sticks are
 * negative up and left, so down is the + half and each right-stick bind
 * has the sign of its left-stick twin. */
#include <stdio.h>
#include <string.h>

extern const char* const input_builtin_autoconfs[];

static const char *const dirs[] = {
   "x_plus", "x_minus", "y_plus", "y_minus" };

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

static int check_profile(const char *conf)
{
   int bad = 0;
   unsigned i;
   int name_len = (int)strcspn(conf, "\n");

   for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
   {
      char l = axis_sign(conf, 'l', dirs[i]);
      char r = axis_sign(conf, 'r', dirs[i]);
      if (!l || l != r)
      {
         printf("FAIL %.*s: r_%s_axis is %c, l_%s_axis is %c\n",
               name_len, conf, dirs[i], r ? r : '?',
               dirs[i], l ? l : '?');
         bad = 1;
      }
   }
   if (axis_sign(conf, 'l', "y_plus") != '+')
   {
      printf("FAIL %.*s: l_y_plus_axis is not +\n", name_len, conf);
      bad = 1;
   }
   return bad;
}

int main(void)
{
   unsigned i;
   unsigned sdl2 = 0, sdl3 = 0;
   int failed = 0;
#ifdef WEBOS
   const unsigned want_sdl2 = 4;
   const char *name = "builtin_autoconf_webos_test";
#else
   const unsigned want_sdl2 = 1;
   const char *name = "builtin_autoconf_test";
#endif

   for (i = 0; input_builtin_autoconfs[i]; i++)
   {
      const char *conf = input_builtin_autoconfs[i];
      if (strstr(conf, "input_driver = \"sdl2\"\n"))
         sdl2++;
      else if (strstr(conf, "input_driver = \"sdl3\"\n"))
         sdl3++;
      else
         continue;
      failed |= check_profile(conf);
   }

   if (sdl2 < want_sdl2 || sdl3 < 2)
   {
      printf("FAIL %u sdl2 and %u sdl3 profiles built in, want %u and 2\n",
            sdl2, sdl3, want_sdl2);
      failed = 1;
   }

   if (failed)
      return 1;
   printf("PASS %s\n", name);
   return 0;
}

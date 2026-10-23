/* Each command-line override must pin the row of its own setting:
 * config_save_file() skips the row carrying a set override, so on the
 * wrong row it drops that setting and saves the command-line value. */
#include "../../../configuration.c"

#include <stdio.h>

struct defaults g_defaults;
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
bool *audio_get_bool_ptr(enum audio_action action)
{
   static bool values[2];
   (void)action;
   return values;
}
bool *video_driver_get_threaded(void)
{
   static bool value;
   return &value;
}
/* Every override is set, as if each was given on the command line. */
bool retroarch_override_setting_is_set(
      enum rarch_override_setting enum_idx, void *data)
{
   (void)data;
   return enum_idx != RARCH_OVERRIDE_SETTING_NONE;
}

struct expect
{
   enum rarch_override_setting o;
   const char *key;
   unsigned seen;
};

static struct expect expects[] = {
   { RARCH_OVERRIDE_SETTING_LOG_TO_FILE,          "log_to_file",          0 },
   { RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT,      "netplay_ip_port",      0 },
   { RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES, "netplay_check_frames", 0 }
};

static int fails;

static void check(const char *kind, enum rarch_override_setting o,
      const char *ident)
{
   unsigned k;
   if (o == RARCH_OVERRIDE_SETTING_NONE)
      return;
   for (k = 0; k < ARRAY_SIZE(expects); k++)
   {
      if (expects[k].o != o)
         continue;
      expects[k].seen++;
      if (strcmp(ident, expects[k].key))
      {
         printf("FAIL: %s override for \"%s\" is on \"%s\"\n",
               kind, expects[k].key, ident);
         fails++;
      }
      return;
   }
   printf("FAIL: %s row \"%s\" has unexpected override %d\n",
         kind, ident, (int)o);
   fails++;
}

int main(void)
{
   static settings_t settings;
   struct config_bool_setting *b;
   struct config_int_setting  *in;
   struct config_uint_setting *u;
   struct config_float_setting *f;
   int n, i;
   unsigned k;

   if (!(b = populate_settings_bool(&settings, &n)))
      return 2;
   for (i = 0; i < n; i++)
      check("bool", b[i].override, b[i].ident);
   free(b);
   if (!(in = populate_settings_int(&settings, &n)))
      return 2;
   for (i = 0; i < n; i++)
      check("int", in[i].override, in[i].ident);
   free(in);
   if (!(u = populate_settings_uint(&settings, &n)))
      return 2;
   for (i = 0; i < n; i++)
      check("uint", u[i].override, u[i].ident);
   free(u);
   if (!(f = populate_settings_float(&settings, &n)))
      return 2;
   for (i = 0; i < n; i++)
      check("float", f[i].override, f[i].ident);
   free(f);

   for (k = 0; k < ARRAY_SIZE(expects); k++)
      if (expects[k].seen != 1)
      {
         printf("FAIL: \"%s\" override pinned %u times\n",
               expects[k].key, expects[k].seen);
         fails++;
      }
   if (fails)
      return 1;
   printf("[pass] cli_override_rows_test\n");
   return 0;
}

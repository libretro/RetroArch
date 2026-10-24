/* configuration.c built without the menu: the float settings that are
 * not the menu's are still loaded and saved, and no setting of any kind
 * is listed twice. */
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
bool retroarch_override_setting_is_set(
      enum rarch_override_setting enum_idx, void *data)
{
   (void)enum_idx;
   (void)data;
   return false;
}

static const char *const wanted[] = {
   "video_aspect_ratio",
   "video_refresh_rate",
   "audio_max_timing_skew",
   "audio_rate_control_delta",
   "fastforward_ratio",
   "slowmotion_ratio",
   "input_analog_deadzone"
};

static int fails;

/* Each of @n identifiers once */
static void unique(const char *kind, const char **ids, int n)
{
   int i, j;
   for (i = 0; i < n; i++)
      for (j = i + 1; j < n; j++)
         if (!strcmp(ids[i], ids[j]))
         {
            printf("FAIL: %s setting \"%s\" listed twice\n", kind, ids[i]);
            fails++;
         }
}

int main(void)
{
   static settings_t settings;
   static const char *ids[4096];
   struct config_bool_setting  *b;
   struct config_int_setting   *in;
   struct config_uint_setting  *u;
   struct config_float_setting *f;
   int n, i;
   unsigned k;

   if (!(f = populate_settings_float(&settings, &n)) || n > 4096)
      return 2;
   for (i = 0; i < n; i++)
      ids[i] = f[i].ident;
   unique("float", ids, n);
   for (k = 0; k < ARRAY_SIZE(wanted); k++)
   {
      for (i = 0; i < n && strcmp(ids[i], wanted[k]); i++) { }
      if (i == n)
      {
         printf("FAIL: float setting \"%s\" is not saved\n", wanted[k]);
         fails++;
      }
   }
   free(f);

   if (!(b = populate_settings_bool(&settings, &n)) || n > 4096)
      return 2;
   for (i = 0; i < n; i++)
      ids[i] = b[i].ident;
   unique("bool", ids, n);
   free(b);
   if (!(in = populate_settings_int(&settings, &n)) || n > 4096)
      return 2;
   for (i = 0; i < n; i++)
      ids[i] = in[i].ident;
   unique("int", ids, n);
   free(in);
   if (!(u = populate_settings_uint(&settings, &n)) || n > 4096)
      return 2;
   for (i = 0; i < n; i++)
      ids[i] = u[i].ident;
   unique("uint", ids, n);
   free(u);

   if (fails)
      return 1;
   printf("[pass] float_rows_test\n");
   return 0;
}

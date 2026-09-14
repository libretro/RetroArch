#include "../../../configuration.c"

/* Runtime defaults unrelated to the HQ setting. */
struct defaults g_defaults;
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

struct hq_setting_row
{
   const char *key;
   bool default_value;
   enum event_command command;
};
#define S_UINT_EX(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, sta, sel, lf, rt, ui, us, sub)
#define S_BOOL(f, T, n, d, sd, df, c, us, sub) { n, d, c },
static const struct hq_setting_row bool_rows[] = {
#include "../../../settings/settings_def_audio_resampler_quality.h"
#include "../../../settings/settings_def_audio_sync.h"
};
#undef S_BOOL
#undef S_UINT_EX

int main(void)
{
   static settings_t settings;
   int i, count, found = 0, transport_found = 0;
   unsigned transport_menu = 0;
   unsigned menu_found = 0;
   struct config_bool_setting *rows = populate_settings_bool(&settings, &count);
   if (!rows || count > SETTINGS_BOOL_COUNT_MAX) return 2;
   for (i = 0; i < count; i++)
      if (!strcmp(rows[i].ident, "audio_resampler_hq_oversampling"))
      {
         if (rows[i].ptr != &settings.bools.audio_resampler_hq_oversampling
               || rows[i].def || !(rows[i].flags & CFG_BOOL_FLG_DEF_ENABLE)) return 3;
         found++;
      }
   for (i = 0; i < count; i++)
      if (!strcmp(rows[i].ident, "audio_time_stretch")
            || !strcmp(rows[i].ident, "audio_time_stretch_lowpass"))
      {
         bool *expected = !strcmp(rows[i].ident, "audio_time_stretch")
               ? &settings.bools.audio_time_stretch : &settings.bools.audio_time_stretch_lowpass;
         if (rows[i].ptr != expected || rows[i].def
               || !(rows[i].flags & CFG_BOOL_FLG_DEF_ENABLE)) return 5;
         transport_found++;
      }
   free(rows);
   for (i = 0; i < (int)ARRAY_SIZE(bool_rows); i++)
      if (!strcmp(bool_rows[i].key, "audio_resampler_hq_oversampling"))
      {
         if (bool_rows[i].default_value || bool_rows[i].command != CMD_EVENT_AUDIO_REINIT)
            return 4;
         menu_found++;
      }
   printf("HQ setting: default off, audio reinit, %d config row, %u descriptor row\n",
         found, menu_found);
   for (i = 0; i < (int)ARRAY_SIZE(bool_rows); i++)
      if (!strcmp(bool_rows[i].key, "audio_time_stretch")
            || !strcmp(bool_rows[i].key, "audio_time_stretch_lowpass"))
      {
         if (bool_rows[i].default_value || bool_rows[i].command != CMD_EVENT_AUDIO_REINIT)
            return 6;
         transport_menu++;
      }
   printf("Transport settings: default off, audio reinit, %d config rows, %u descriptor rows\n",
         transport_found, transport_menu);
   return found != 1 || menu_found != 1 || transport_found != 2 || transport_menu != 2;
}

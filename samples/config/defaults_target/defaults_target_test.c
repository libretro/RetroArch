/* config_set_defaults() populates the struct it is handed and only
 * that struct: config_get_ptr() stays pointer-stable and untouched,
 * which is what lets config_save_file() build its minimal-mode
 * defaults without ever swapping the pointer other threads read. */
#include "../../../configuration.c"
#include "../../../libretro-common/compat/compat_strl.c"

#include <stdio.h>

struct defaults g_defaults;
static global_t test_global;

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
void video_driver_set_threaded(bool val) { (void)val; }
void audio_set_float(enum audio_action action, float val) { (void)action; (void)val; }
const char *config_get_default_ui_companion(void) { return "null"; }
void dir_clear(enum rarch_dir_type type) { (void)type; }
void dir_set(enum rarch_dir_type type, const char *path) { (void)type; (void)path; }
bool path_set(enum rarch_path_type type, const char *path) { (void)type; (void)path; return true; }
size_t fill_pathname_expand_special(char *out, const char *in, size_t size)
{ return strlcpy(out, in, size); }
size_t fill_pathname_join(char *out, const char *dir, const char *path, size_t size)
{
   /* Alias-safe like the real helper: @out may be @dir. */
   size_t n = strlen(dir);
   if (size)
   {
      if (n > size - 1)
         n = size - 1;
      memmove(out, dir, n);
      out[n] = '\0';
   }
   else
      n = 0;
   return n + strlcpy(out + n, path, size ? size - n : 0);
}
void input_config_reset(void) { }
void input_config_set_device(unsigned port, unsigned id) { (void)port; (void)id; }
void input_remapping_deinit(bool save) { (void)save; }
void input_remapping_set_defaults(bool clear) { (void)clear; }
bool path_is_directory(const char *path) { (void)path; return false; }
bool path_mkdir(const char *dir) { (void)dir; return true; }
recording_state_t *recording_state_get_ptr(void)
{ static recording_state_t st; return &st; }
bool retroarch_ctl(enum rarch_ctl_state state, void *data)
{ (void)state; (void)data; return false; }
bool retroarch_override_setting_is_set(enum rarch_override_setting enum_idx, void *data)
{ (void)enum_idx; (void)data; return false; }

int main(void)
{
   settings_t *live;
   settings_t *target = (settings_t*)calloc(1, sizeof(settings_t));
   if (!target) return 2;

   retroarch_config_init();
   live = config_get_ptr();
   if (!live) { printf("FAIL: no live settings\n"); return 1; }

   /* A sentinel a default would overwrite. */
   live->bools.audio_enable        = false;
   live->floats.slowmotion_ratio   = 7.0f;

   config_set_defaults(&test_global, target);

   if (config_get_ptr() != live)
   { printf("FAIL: defaults moved config_get_ptr()\n"); return 1; }
   if (live->bools.audio_enable || live->floats.slowmotion_ratio != 7.0f)
   { printf("FAIL: defaults wrote through the live struct\n"); return 1; }
   if (!target->bools.audio_enable)
   { printf("FAIL: the target did not receive the defaults\n"); return 1; }

   printf("defaults target: the live settings never move, the target fills\n");
   free(target);
   retroarch_config_deinit();
   return 0;
}

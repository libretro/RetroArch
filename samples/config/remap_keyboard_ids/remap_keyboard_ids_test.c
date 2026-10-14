/* input_remapping_load_file() stores a keyboard mapping only when it is
 * a key the mapper's arrays have room for: anything below RETROK_UNKNOWN
 * or from RETROK_LAST up is stored as RETROK_UNKNOWN, which is what lets
 * the poll index those arrays by the stored key unchecked. */
#include "../../../configuration.c"
#include "../../../libretro-common/compat/compat_strl.c"
#include "../../../libretro-common/file/config_file.c"

#include <stdio.h>

struct defaults g_defaults;

static runloop_state_t test_runloop;
runloop_state_t *runloop_state_get_ptr(void) { return &test_runloop; }
void input_remapping_deinit(bool save) { (void)save; }
void input_remapping_set_defaults(bool clear) { (void)clear; }
retro_atomic_int_t input_binds_generation;
void input_binds_kept_invalidate(void) { }
void path_linked_list_free(struct path_linked_list *l) { (void)l; }
void path_linked_list_add_path(struct path_linked_list *l, char *p)
{ (void)l; (void)p; }
void fill_pathname_resolve_relative(char *s, const char *ref,
      const char *in, size_t len) { (void)ref; strlcpy(s, in, len); }
size_t fill_pathname_abbreviated_or_relative(char *s, const char *ref,
      const char *in, size_t len) { (void)ref; return strlcpy(s, in, len); }
void input_driver_set_remapping_cache_active(void) { }
void input_remapping_update_port_map(void) { }
size_t fill_pathname_join_delim(char *s, const char *dir,
      const char *path, const char delim, size_t len)
{
   return (size_t)snprintf(s, len, "%s%c%s", dir, delim, path);
}

static int failures = 0;
#define CHECK(c) do { if (!(c)) { \
   printf("FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(void)
{
   config_file_t *conf;
   settings_t *settings;
   char text[512];

   retroarch_config_init();
   settings = config_get_ptr();

   snprintf(text, sizeof(text),
         "input_player1_key_a = \"%d\"\n"
         "input_player1_key_b = \"%d\"\n"
         "input_player1_key_x = \"-1\"\n"
         "input_player1_key_y = \"%d\"\n"
         "input_player2_key_l_x+ = \"%d\"\n"
         "input_player2_key_start = \"999999\"\n"
         "input_player2_key_r_y- = \"%d\"\n",
         (int)RETROK_SPACE, (int)RETROK_LAST - 1,
         (int)RETROK_LAST, (int)RETROK_a, (int)RETROK_LAST + 7);
   conf = config_file_new_from_string(text, "test.rmp");
   CHECK(conf != NULL);
   CHECK(input_remapping_load_file(conf, "test.rmp"));

   /* keys the mapper holds are kept */
   CHECK(settings->uints.input_keymapper_ids[0][RETRO_DEVICE_ID_JOYPAD_A]
         == RETROK_SPACE);
   CHECK(settings->uints.input_keymapper_ids[0][RETRO_DEVICE_ID_JOYPAD_B]
         == RETROK_LAST - 1);
   CHECK(settings->uints.input_keymapper_ids[1][RARCH_ANALOG_LEFT_X_PLUS]
         == RETROK_a);
   /* anything else is no key at all, on the button and stick paths */
   CHECK(settings->uints.input_keymapper_ids[0][RETRO_DEVICE_ID_JOYPAD_X]
         == RETROK_UNKNOWN);
   CHECK(settings->uints.input_keymapper_ids[0][RETRO_DEVICE_ID_JOYPAD_Y]
         == RETROK_UNKNOWN);
   CHECK(settings->uints.input_keymapper_ids[1][RETRO_DEVICE_ID_JOYPAD_START]
         == RETROK_UNKNOWN);
   CHECK(settings->uints.input_keymapper_ids[1][RARCH_ANALOG_RIGHT_Y_MINUS]
         == RETROK_UNKNOWN);

   config_file_free(conf);
   free(test_runloop.name.remapfile);
   retroarch_config_deinit();
   printf("remap keyboard ids: %s (%d failures)\n",
         failures ? "FAIL" : "PASS", failures);
   return failures ? 1 : 0;
}

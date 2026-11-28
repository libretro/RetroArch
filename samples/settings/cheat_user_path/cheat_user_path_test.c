/* cheat_user_path: its row, an empty value surviving a save and load,
 * and the one-time rule that leaves User Cheat Files off for a config
 * saved without the key whose Cheat Files is a folder of its own,
 * which an override load leaves alone. */
#include "../../../configuration.c"
#include "../../../libretro-common/compat/fopen_utf8.c"
#include "../../../libretro-common/compat/compat_strl.c"
#include "../../../libretro-common/compat/compat_strcasestr.c"
#include "../../../libretro-common/compat/compat_posix_string.c"
#include "../../../libretro-common/encodings/encoding_utf.c"
#include "../../../libretro-common/file/file_path.c"
#include "../../../libretro-common/file/file_path_io.c"
#include "../../../libretro-common/file/config_file.c"
#include "../../../libretro-common/file/config_file_io.c"
#include "../../../libretro-common/lists/string_list.c"
#include "../../../libretro-common/streams/file_stream.c"
#include "../../../libretro-common/string/rstrtod.c"
#include "../../../libretro-common/vfs/vfs_implementation.c"
#include "../../../libretro-common/time/rtime.c"

#include <stdio.h>

#define APP_CHEATS    "/app/cheats"
#define SHARED_CHEATS "/shared/cheats"
#define TEST_CFG      "cheat_user_path_test.cfg"
#define TEST_OVERRIDE "cheat_user_path_override.cfg"

struct defaults g_defaults;
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
recording_state_t *recording_state_get_ptr(void)
{
   static recording_state_t st;
   return &st;
}
char *dir_get_ptr(enum rarch_dir_type type)
{
   static char dir[PATH_MAX_LENGTH];
   (void)type;
   return dir;
}

/* What config_load_file() calls outside configuration.c */
static char test_paths[RARCH_PATH_CONFIG_DEFAULT + 1][PATH_MAX_LENGTH];
static runloop_state_t test_runloop;
static bool test_bool;
retro_atomic_int_t input_binds_generation;
bool path_set(enum rarch_path_type type, const char *path)
{
   strlcpy(test_paths[type], path, sizeof(test_paths[type]));
   return true;
}
const char *path_get(enum rarch_path_type type) { return test_paths[type]; }
bool path_is_empty(enum rarch_path_type type) { return !*test_paths[type]; }
runloop_state_t *runloop_state_get_ptr(void) { return &test_runloop; }
uint32_t retroarch_get_flags(void) { return 0; }
bool retroarch_override_setting_is_set(enum rarch_override_setting s,
      void *data) { return false; }
bool retroarch_ctl(enum rarch_ctl_state state, void *data) { return false; }
bool *audio_get_bool_ptr(enum audio_action action) { return &test_bool; }
bool *video_driver_get_threaded(void) { return &test_bool; }
void audio_set_float(enum audio_action action, float val) { }
void config_read_keybinds_conf(void *data) { }
void dir_set(enum rarch_dir_type type, const char *path) { }
bool fill_pathname_application_data(char *s, size_t len) { return false; }
enum retro_language frontend_driver_get_user_language(void)
{
   return RETRO_LANGUAGE_ENGLISH;
}
bool frontend_driver_has_gamemode(void) { return false; }
bool frontend_driver_set_gamemode(bool on) { return false; }
bool frontend_driver_set_power_plan(bool on, bool idle_disable) { return true; }
void frontend_driver_set_sustained_performance_mode(bool on) { }
void input_binds_kept_invalidate(void) { }
bool input_config_sanitize_joypad_indices(void) { return false; }
void input_driver_deadzones_refresh(void) { }
void input_plays_as_read(void *conf) { }
void input_plays_as_merge(void *conf) { }
void input_plays_as_hand_out(void) { }
void msg_hash_set_uint(enum msg_hash_action type, unsigned val) { }
void RARCH_LOG(const char *fmt, ...) { }
void RARCH_WARN(const char *fmt, ...) { }
void recording_driver_update_streaming_url(void) { }
void verbosity_enable(void) { }
void verbosity_disable(void) { }
void verbosity_set_log_level(unsigned level) { }
void video_views_xr_reset(video_screen_xr_t *screens, unsigned count) { }
void video_views_xr_normalize(video_screen_xr_t *screen) { }

static settings_t settings;
static int failures = 0;
static const char *test_name;

#define CHECK(c) do { if (!(c)) { \
   printf("FAIL %s:%d: %s\n", test_name, __LINE__, #c); failures++; } } while (0)

/* As config_set_defaults() leaves them, then Cheat Files as loaded */
static void setup(const char *user_default, const char *cheat_files)
{
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CHEATS], APP_CHEATS,
         sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CHEATS_USER], user_default,
         sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS_USER]));
   strlcpy(settings.paths.path_cheat_user, user_default,
         sizeof(settings.paths.path_cheat_user));
   strlcpy(settings.paths.path_cheat_database, cheat_files,
         sizeof(settings.paths.path_cheat_database));
}

static void run_rule(const char *text)
{
   char buf[256];
   config_file_t *conf;
   strlcpy(buf, text, sizeof(buf));
   conf = config_file_new_from_string(buf, NULL);
   CHECK(conf != NULL);
   if (!conf)
      return;
   config_load_cheat_user_path(conf, &settings);
   config_file_free(conf);
}

static void test_row(void)
{
   int n = SETTINGS_PATH_COUNT_MAX;
   int i;
   int found = 0;
   struct config_path_setting *paths;
   test_name = "test_row";
   paths = populate_settings_path(&settings, &n);
   CHECK(paths != NULL);
   if (!paths)
      return;
   for (i = 0; i < n; i++)
   {
      if (strcmp(paths[i].ident, "cheat_user_path"))
         continue;
      found++;
      CHECK(paths[i].ptr == settings.paths.path_cheat_user);
      CHECK(paths[i].flags & CFG_BOOL_FLG_HANDLE);
      CHECK(!(paths[i].flags & CFG_BOOL_FLG_DEF_ENABLE));
   }
   CHECK(found == 1);
   free(paths);
}

static void test_empty_round_trip(void)
{
   char value[64];
   config_file_t *conf = config_file_new_alloc();
   test_name = "test_empty_round_trip";
   CHECK(conf != NULL);
   if (!conf)
      return;
   config_set_path(conf, "cheat_user_path", "");
   CHECK(config_file_write(conf, TEST_CFG, true));
   config_file_free(conf);

   conf = config_file_new(TEST_CFG);
   CHECK(conf != NULL);
   if (conf)
   {
      strlcpy(value, "unset", sizeof(value));
      CHECK(config_get_path(conf, "cheat_user_path", value, sizeof(value)));
      CHECK(!strcmp(value, ""));
      config_file_free(conf);
   }
   filestream_delete(TEST_CFG);
}

static void test_rule_default_cheats(void)
{
   test_name = "test_rule_default_cheats";
   setup(SHARED_CHEATS, APP_CHEATS);
   run_rule("cheat_database_path = \"" APP_CHEATS "\"\n");
   CHECK(!strcmp(settings.paths.path_cheat_user, SHARED_CHEATS));
}

static void test_rule_shared_cheats(void)
{
   test_name = "test_rule_shared_cheats";
   setup(SHARED_CHEATS, SHARED_CHEATS);
   run_rule("cheat_database_path = \"" SHARED_CHEATS "\"\n");
   CHECK(!strcmp(settings.paths.path_cheat_user, SHARED_CHEATS));
}

/* A hand-written trailing separator names the same folder */
static void test_rule_trailing_slash(void)
{
   test_name = "test_rule_trailing_slash";
   setup(SHARED_CHEATS, SHARED_CHEATS "/");
   run_rule("cheat_database_path = \"" SHARED_CHEATS "/\"\n");
   CHECK(!strcmp(settings.paths.path_cheat_user, SHARED_CHEATS));
   setup(SHARED_CHEATS, APP_CHEATS "//");
   run_rule("cheat_database_path = \"" APP_CHEATS "//\"\n");
   CHECK(!strcmp(settings.paths.path_cheat_user, SHARED_CHEATS));
   setup(SHARED_CHEATS, APP_CHEATS "x/");
   run_rule("cheat_database_path = \"" APP_CHEATS "x/\"\n");
   CHECK(!strcmp(settings.paths.path_cheat_user, ""));
}

static void test_rule_elsewhere(void)
{
   test_name = "test_rule_elsewhere";
   setup(SHARED_CHEATS, "/old/cheats");
   run_rule("cheat_database_path = \"/old/cheats\"\n");
   CHECK(!strcmp(settings.paths.path_cheat_user, ""));
}

static void test_rule_key_empty(void)
{
   test_name = "test_rule_key_empty";
   setup(SHARED_CHEATS, APP_CHEATS);
   *settings.paths.path_cheat_user = '\0';
   run_rule("cheat_user_path = \"\"\n");
   CHECK(!strcmp(settings.paths.path_cheat_user, ""));
}

static void test_rule_key_set(void)
{
   test_name = "test_rule_key_set";
   setup(SHARED_CHEATS, "/old/cheats");
   strlcpy(settings.paths.path_cheat_user, "/x",
         sizeof(settings.paths.path_cheat_user));
   run_rule("cheat_database_path = \"/old/cheats\"\n"
         "cheat_user_path = \"/x\"\n");
   CHECK(!strcmp(settings.paths.path_cheat_user, "/x"));
}

static void test_rule_not_android(void)
{
   test_name = "test_rule_not_android";
   setup("", "/old/cheats");
   run_rule("cheat_database_path = \"/old/cheats\"\n");
   CHECK(!strcmp(settings.paths.path_cheat_user, ""));
   /* Untouched: a value already there stays */
   strlcpy(settings.paths.path_cheat_user, "/x",
         sizeof(settings.paths.path_cheat_user));
   run_rule("cheat_database_path = \"/old/cheats\"\n");
   CHECK(!strcmp(settings.paths.path_cheat_user, "/x"));
   CHECK(!strcmp(settings.paths.path_cheat_database, "/old/cheats"));
}

static void write_text(const char *path, const char *text)
{
   CHECK(filestream_write_file(path, text, strlen(text)));
}

/* First session after the upgrade: an override that moves Cheat Files
 * keeps the user folder, which a later save would write back */
static void test_rule_not_on_override(void)
{
   test_name = "test_rule_not_on_override";
   setup(SHARED_CHEATS, APP_CHEATS);
   write_text(TEST_CFG, "cheat_database_path = \"" APP_CHEATS "\"\n");
   write_text(TEST_OVERRIDE, "cheat_database_path = \"/old/cheats\"\n");
   CHECK(config_load_file(TEST_CFG, &settings));
   CHECK(!strcmp(settings.paths.path_cheat_user, SHARED_CHEATS));

   path_set(RARCH_PATH_CONFIG_OVERRIDE, TEST_OVERRIDE);
   CHECK(config_load_file(TEST_CFG, &settings));
   CHECK(!strcmp(settings.paths.path_cheat_database, "/old/cheats"));
   CHECK(!strcmp(settings.paths.path_cheat_user, SHARED_CHEATS));
   path_set(RARCH_PATH_CONFIG_OVERRIDE, "");

   /* Without overrides, the main config's own Cheat Files decides */
   write_text(TEST_CFG, "cheat_database_path = \"/old/cheats\"\n");
   CHECK(config_load_file(TEST_CFG, &settings));
   CHECK(!strcmp(settings.paths.path_cheat_user, ""));

   filestream_delete(TEST_CFG);
   filestream_delete(TEST_OVERRIDE);
}

static void test_save_keeps_key(void)
{
   char value[64];
   config_file_t *conf;
   test_name = "test_save_keeps_key";

   /* Minimal mode dropped the key: it equals the default */
   setup(SHARED_CHEATS, APP_CHEATS);
   conf = config_file_new_alloc();
   CHECK(conf != NULL);
   if (!conf)
      return;
   config_save_cheat_user_path(conf, &settings);
   strlcpy(value, "unset", sizeof(value));
   CHECK(config_get_path(conf, "cheat_user_path", value, sizeof(value)));
   CHECK(!strcmp(value, SHARED_CHEATS));
   config_file_free(conf);

   /* The path loop's value, without its trailing separator, stays */
   strlcpy(settings.paths.path_cheat_user, SHARED_CHEATS "/",
         sizeof(settings.paths.path_cheat_user));
   conf = config_file_new_alloc();
   CHECK(conf != NULL);
   if (!conf)
      return;
   config_set_path(conf, "cheat_user_path", SHARED_CHEATS);
   config_save_cheat_user_path(conf, &settings);
   strlcpy(value, "unset", sizeof(value));
   CHECK(config_get_path(conf, "cheat_user_path", value, sizeof(value)));
   CHECK(!strcmp(value, SHARED_CHEATS));
   config_file_free(conf);

   /* No user default: nothing is added */
   setup("", APP_CHEATS);
   conf = config_file_new_alloc();
   CHECK(conf != NULL);
   if (!conf)
      return;
   config_save_cheat_user_path(conf, &settings);
   CHECK(config_get_entry(conf, "cheat_user_path") == NULL);
   config_file_free(conf);
}

int main(void)
{
   test_row();
   test_empty_round_trip();
   test_rule_default_cheats();
   test_rule_shared_cheats();
   test_rule_trailing_slash();
   test_rule_elsewhere();
   test_rule_key_empty();
   test_rule_key_set();
   test_rule_not_android();
   test_rule_not_on_override();
   test_save_keeps_key();
   if (failures)
   {
      printf("cheat_user_path: FAIL (%d failures)\n", failures);
      return 1;
   }
   printf("cheat_user_path: PASS (0 failures)\n");
   return 0;
}

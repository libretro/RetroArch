/* Include the shipping loop to exercise its private deferred dispatcher. */
#define runloop_msg_queue_push auto_state_msg_queue_push
#include "../../../runloop.c"
#undef runloop_msg_queue_push
#include <assert.h>
#include <limits.h>
#include "../../../runtime_file.h"
#include "../../../list_special.h"

static settings_t test_settings;
static video_driver_state_t test_video;
static unsigned runs, size_checks, loads, settings_checks, path_checks;
static unsigned initialize_after;
static bool rolling;
static bool runtime_present;
static unsigned runtime_slot;
static unsigned file_count;
static char files[32][256];
static struct string_list directory;
static struct string_list_elem entries[32];
static retro_task_callback_t save_done;
static void *save_userdata;
static char saved_path[256];
static char loaded_path[256];
static bool accept_save = true;
static bool accept_ram_save = true;
static const char *delete_failure;
static const char *pending_path;
static unsigned rolling_failures;

#define CHECK_ROLLING(condition) do { \
   if (!(condition)) { \
      fprintf(stderr, "rolling line %d: %s\n", __LINE__, #condition); \
      rolling_failures++; \
   } \
} while (0)

static bool has_file(const char *path)
{
   unsigned i;
   for (i = 0; i < file_count; i++)
      if (!strcmp(files[i], path))
         return true;
   return false;
}

static void add_file(const char *path)
{
   assert(file_count < 32);
   if (!has_file(path))
      strlcpy(files[file_count++], path, sizeof(files[0]));
}
#ifdef HAVE_NETWORKING
static bool allow_frame = true;
bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data)
{
   (void)data;
   return state == RARCH_NETPLAY_CTL_PRE_FRAME ? allow_frame : false;
}
#endif
#ifdef HAVE_CHEEVOS
static bool hardcore;
bool rcheevos_hardcore_active(void) { return hardcore; }
void rcheevos_pause_hardcore(void) { hardcore = false; }
#endif

settings_t *config_get_ptr(void)
{
   settings_checks++;
   return &test_settings;
}

video_driver_state_t *video_state_get_ptr(void) { return &test_video; }
void input_driver_poll(void) {}
void input_driver_poll_between_frames(void (*poll)(void)) { poll(); }
void video_driver_cached_frame(void) {}
void audio_driver_frame_end(void) {}
void recording_start_frame(void) {}
void audio_driver_publish_runloop(void) {}
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
bool verbosity_is_enabled(void) { return false; }
bool core_info_current_supports_savestate(void) { return true; }

static void mock_run(void) { runs++; }
static size_t mock_size(void)
{
   size_checks++;
   return runs >= initialize_after ? 16 : 0;
}

bool content_load_state(const char *path, bool load_to_backup, bool autoload)
{
   if (rolling)
   {
      strlcpy(loaded_path, path, sizeof(loaded_path));
      return has_file(path);
   }
   assert(strcmp(path, "test.auto") == 0);
   assert(!load_to_backup && autoload);
   loads++;
   return true;
}

bool path_is_valid(const char *path)
{
   if (rolling)
      return !strcmp(path, "test.lrtl") ? runtime_present : has_file(path);
   assert(strcmp(path, "test.auto") == 0);
   path_checks++;
   return true;
}

struct string_list *dir_list_new_special(const char *path,
      enum dir_list_type type, const char *filter, bool hidden)
{
   unsigned i;
   (void)path;
   (void)filter;
   (void)hidden;
   assert(type == DIR_LIST_PLAIN);
   directory.elems = entries;
   directory.size = file_count;
   for (i = 0; i < file_count; i++)
   {
      entries[i].data = files[i];
      entries[i].attr.i = 0;
   }
   return &directory;
}

void string_list_free(struct string_list *list) { assert(list == &directory); }
void dir_list_free(struct string_list *list) { string_list_free(list); }

int filestream_delete(const char *path)
{
   unsigned i;
   if (delete_failure && !strcmp(delete_failure, path))
      return -1;
   for (i = 0; i < file_count; i++)
      if (!strcmp(files[i], path))
      {
         for (; i + 1 < file_count; i++)
            strcpy(files[i], files[i + 1]);
         file_count--;
         return 0;
      }
   return -1;
}

bool content_save_state_notify(const char *path, bool disk,
      retro_task_callback_t cb, void *userdata)
{
   assert(disk && !save_done);
   if (!accept_save)
      return false;
   strlcpy(saved_path, path, sizeof(saved_path));
   save_done = cb;
   save_userdata = userdata;
   return true;
}

bool content_save_state(const char *path, bool disk)
{
   return content_save_state_notify(path, disk, NULL, NULL);
}
bool content_save_state_to_ram(void) { return accept_ram_save; }
bool content_state_file_in_use(const char *path)
{
   return pending_path && !strcmp(pending_path, path);
}
bool content_load_state_from_ram(void) { return true; }
bool content_undo_load_state(void) { return false; }
bool content_undo_save_state(void) { return false; }
bool content_undo_load_buf_is_empty(void) { return true; }
bool content_undo_save_buf_is_empty(void) { return true; }
bool content_undo_save_disabled(void) { return false; }

retro_time_t cpu_features_get_time_usec(void) { return 1000000; }
const char *path_get(enum rarch_path_type type)
{
   return type == RARCH_PATH_CONTENT ? "test.gba" : "testcore.so";
}
runtime_log_t *runtime_log_init(const char *content, const char *core,
      const char *runtime, const char *playlist, bool per_core)
{
   runtime_log_t *log = (runtime_log_t*)calloc(1, sizeof(*log));
   (void)content;
   (void)core;
   (void)runtime;
   (void)playlist;
   (void)per_core;
   assert(log);
   log->state_slot = runtime_slot;
   strcpy(log->path, "test.lrtl");
   return log;
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned priority, unsigned duration, bool flush,
      char *title, enum message_queue_icon icon,
      enum message_queue_category category)
{
   (void)msg; (void)len; (void)priority; (void)duration;
   (void)flush; (void)title; (void)icon; (void)category;
}

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   (void)msg;
   return "test";
}

static void reset(unsigned frames, bool quirk)
{
   rolling = false;
   memset(&runloop_state, 0, sizeof(runloop_state));
   memset(&test_settings, 0, sizeof(test_settings));
   memset(&test_video, 0, sizeof(test_video));
   test_settings.bools.savestate_auto_load = true;
   runloop_state.current_core.flags = RETRO_CORE_FLAG_GAME_LOADED;
   runloop_state.current_core.retro_run = mock_run;
   runloop_state.current_core.retro_serialize_size = mock_size;
   runloop_state.current_core.serialization_quirks_v = quirk
      ? RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE : 0;
   strcpy(runloop_state.name.savestate, "test");
   runs = size_checks = loads = settings_checks = path_checks = 0;
   initialize_after = frames;
}

static void rolling_reset(void)
{
   reset(0, false);
   rolling = true;
   file_count = 0;
   save_done = NULL;
   save_userdata = NULL;
   saved_path[0] = loaded_path[0] = '\0';
   accept_save = true;
   accept_ram_save = true;
   delete_failure = NULL;
   runtime_present = true;
   runtime_slot = 9;
   strcpy(runloop_state.name.savestate, "test.state");
   runloop_state.entry_state_slot = -1;
   test_settings.bools.savestate_auto_index = true;
   test_settings.bools.content_runtime_log = true;
   test_settings.uints.savestate_max_keep = 10;
}

static void add_slot(unsigned slot)
{
   char path[256];
   runloop_get_savestate_path(path, sizeof(path), (int)slot);
   add_file(path);
}

static void fill_ring(void)
{
   unsigned i;
   for (i = 0; i < 10; i++)
      add_slot(i);
}

static void finish_save(bool success)
{
   retro_task_callback_t cb = save_done;
   void *userdata = save_userdata;
   save_done = NULL;
   save_userdata = NULL;
   if (success)
      add_file(saved_path);
   if (cb)
      cb(NULL, saved_path, userdata, success ? NULL : "write failed");
}

static void test_rolling_states(void)
{
   unsigned i;
   rolling_reset();
   for (i = 0; i <= 10; i++)
      if (i != 9)
         add_slot(i);
   command_event_set_savestate_auto_index(&test_settings);
   runloop_runtime_log_init(&runloop_state);
   CHECK_ROLLING(test_settings.ints.state_slot == 8);
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_LOAD_STATE));
   CHECK_ROLLING(!strcmp(loaded_path, "test.state8"));

   runtime_slot = 3;
   runloop_runtime_log_init(&runloop_state);
   CHECK_ROLLING(test_settings.ints.state_slot == 3);
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_LOAD_STATE));
   CHECK_ROLLING(!strcmp(loaded_path, "test.state3"));
   runtime_slot = 0;
   runloop_runtime_log_init(&runloop_state);
   CHECK_ROLLING(test_settings.ints.state_slot == 0);
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_LOAD_STATE));
   CHECK_ROLLING(!strcmp(loaded_path, "test.state"));

   runloop_state.entry_state_slot = 8;
   test_settings.ints.state_slot = 8;
   runtime_slot = 3;
   runloop_runtime_log_init(&runloop_state);
   CHECK_ROLLING(test_settings.ints.state_slot == 8);
   runloop_state.entry_state_slot = -1;
   test_settings.bools.savestate_auto_index = false;
   runtime_slot = 9;
   runloop_runtime_log_init(&runloop_state);
   CHECK_ROLLING(test_settings.ints.state_slot == 9);

   rolling_reset();
   for (i = 1; i <= 9; i++)
      add_slot(i);
   command_event_set_savestate_auto_index(&test_settings);
   CHECK_ROLLING(test_settings.ints.state_slot == 9);
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_LOAD_STATE));
   CHECK_ROLLING(!strcmp(loaded_path, "test.state9"));

   rolling_reset();
   add_slot(3);
   add_slot(6);
   add_file("test.stateExtra.state8");
   add_file("test.statejunk7");
   add_file("test.state2147483648");
   add_file("test.state01");
   add_file("test.state0");
   add_file("test.state.tmp");
   add_file("test.state.auto");
   command_event_set_savestate_auto_index(&test_settings);
   CHECK_ROLLING(test_settings.ints.state_slot == 6);

   rolling_reset();
   test_settings.uints.savestate_max_keep = 0;
   add_slot(3);
   add_slot(12);
   command_event_set_savestate_auto_index(&test_settings);
   CHECK_ROLLING(test_settings.ints.state_slot == 12);
   runtime_slot = 9;
   runloop_runtime_log_init(&runloop_state);
   CHECK_ROLLING(test_settings.ints.state_slot == 12);
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_LOAD_STATE));
   CHECK_ROLLING(!strcmp(loaded_path, "test.state12"));
   test_settings.ints.state_slot = INT_MAX;
   CHECK_ROLLING(command_event_get_next_savestate_auto_index(&test_settings) == 0);
   test_settings.ints.state_slot = -1;
   CHECK_ROLLING(command_event_get_next_savestate_auto_index(&test_settings) == 0);

   rolling_reset();
   memset(runloop_state.name.savestate, 'g', 150);
   strcpy(runloop_state.name.savestate + 150, ".state");
   add_slot(3);
   command_event_set_savestate_auto_index(&test_settings);
   CHECK_ROLLING(test_settings.ints.state_slot == 3);
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_LOAD_STATE));
   CHECK_ROLLING(has_file(loaded_path));

   rolling_reset();
   fill_ring();
   test_settings.ints.state_slot = 10;
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE));
   CHECK_ROLLING(has_file("test.state") && file_count == 10);
   test_settings.ints.state_slot = 4;
   finish_save(true);
   CHECK_ROLLING(!has_file("test.state") && has_file("test.state10"));
   CHECK_ROLLING(file_count == 10 && test_settings.ints.state_slot == 4);

   rolling_reset();
   fill_ring();
   test_settings.ints.state_slot = 10;
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE));
   finish_save(false);
   CHECK_ROLLING(has_file("test.state") && file_count == 10);
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE));
   finish_save(true);
   CHECK_ROLLING(!has_file("test.state") && has_file("test.state10"));
   CHECK_ROLLING(file_count == 10);

   rolling_reset();
   fill_ring();
   test_settings.ints.state_slot = 9;
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE));
   finish_save(true);
   CHECK_ROLLING(has_file("test.state") && file_count == 10);
   command_event_set_savestate_auto_index(&test_settings);
   CHECK_ROLLING(test_settings.ints.state_slot == 9);
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE_TO_RAM));
   CHECK_ROLLING(has_file("test.state") && file_count == 10);
   accept_ram_save = false;
   CHECK_ROLLING(!command_event_main_state(CMD_EVENT_SAVE_STATE_TO_RAM));
   CHECK_ROLLING(test_settings.ints.state_slot == 9);
   CHECK_ROLLING(has_file("test.state") && file_count == 10);
   accept_ram_save = true;
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE_TO_RAM));
   CHECK_ROLLING(test_settings.ints.state_slot == 9 && file_count == 10);
   accept_save = false;
   CHECK_ROLLING(!command_event_main_state(CMD_EVENT_SAVE_STATE));
   CHECK_ROLLING(has_file("test.state") && file_count == 10);
   rolling_reset();
   fill_ring();
   add_file("test.state.png");
   delete_failure = "test.state";
   test_settings.ints.state_slot = 10;
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE));
   finish_save(true);
   CHECK_ROLLING(has_file("test.state") && has_file("test.state.png"));
   CHECK_ROLLING(has_file("test.state10"));
   delete_failure = NULL;
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE));
   finish_save(true);
   CHECK_ROLLING(!has_file("test.state") && !has_file("test.state.png"));
   CHECK_ROLLING(file_count == 10);
   rolling_reset();
   fill_ring();
   test_settings.ints.state_slot = 10;
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE));
   pending_path = "test.state";
   finish_save(true);
   CHECK_ROLLING(has_file("test.state") && file_count == 11);
   test_settings.ints.state_slot = 0;
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_LOAD_STATE));
   pending_path = NULL;
   test_settings.ints.state_slot = 10;
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE));
   finish_save(true);
   CHECK_ROLLING(!has_file("test.state") && file_count == 10);
   rolling_reset();
   fill_ring();
   test_settings.ints.state_slot = 10;
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE));
   strcpy(runloop_state.name.savestate, "other.state");
   test_settings.uints.savestate_max_keep = 100;
   test_settings.ints.state_slot = 2;
   add_slot(0);
   add_slot(1);
   finish_save(true);
   CHECK_ROLLING(!has_file("test.state") && has_file("test.state10"));
   CHECK_ROLLING(has_file("other.state") && has_file("other.state1"));
   CHECK_ROLLING(test_settings.ints.state_slot == 2);
   CHECK_ROLLING(test_settings.uints.savestate_max_keep == 100);
   test_settings.uints.savestate_max_keep = 2;
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_SAVE_STATE));
   finish_save(true);
   CHECK_ROLLING(!has_file("other.state") && has_file("other.state2"));
   CHECK_ROLLING(has_file("test.state10"));
   CHECK_ROLLING(command_event_main_state(CMD_EVENT_LOAD_STATE));
   CHECK_ROLLING(!strcmp(loaded_path, "other.state2"));
   rolling = false;
}

int main(void)
{
   unsigned i;
   reset(3, true);
   assert(command_event_load_auto_state());
   runloop_load_deferred_auto_state();
   assert(loads == 0 && size_checks == 0);
   for (i = 0; i < 3; i++)
   {
      core_run();
      runloop_load_deferred_auto_state();
      assert(loads == (i == 2 ? 1U : 0U));
      assert(runloop_state.auto_state_load_pending == (i != 2));
      assert(path_checks == (i == 2 ? 2U : 1U));
   }
   assert(size_checks == 3);
   settings_checks = 0;
   for (i = 0; i < 10; i++)
   {
      core_run();
      runloop_load_deferred_auto_state();
   }
   assert(loads == 1 && size_checks == 3 && settings_checks == 0);
   assert(!command_event_load_auto_state());
   assert(loads == 1 && path_checks == 2);

   reset(1, true);
   assert(command_event_load_auto_state());
   runloop_state.current_core.retro_run = NULL;
   core_run();
   runloop_load_deferred_auto_state();
   assert(loads == 0 && size_checks == 0);

   reset(3, true);
   assert(command_event_load_auto_state());
   test_settings.bools.savestate_auto_load = false;
   runloop_load_deferred_auto_state();
   assert(!runloop_state.auto_state_load_pending && loads == 0);

   reset(3, true);
   assert(command_event_load_auto_state());
   runloop_state.content_closing = true;
   core_run();
   runloop_load_deferred_auto_state();
   assert(runs == 0 && loads == 0 && !runloop_state.auto_state_load_pending);

   reset(3, true);
   assert(command_event_load_auto_state());
   runloop_state.current_core.flags = 0;
   runloop_load_deferred_auto_state();
   assert(loads == 0 && !runloop_state.auto_state_load_pending);

   reset(3, true);
   assert(command_event_load_auto_state());
   core_run();
   runloop_reset_auto_state_load(&runloop_state);
   assert(!runloop_state.auto_state_load_pending);
   assert(!runloop_state.auto_state_load_attempted);
   assert(!runloop_state.auto_state_load_ready);
   assert(!runloop_state.auto_state_load_core_ran);
   assert(command_event_load_auto_state());
   runloop_load_deferred_auto_state();
   assert(loads == 0 && size_checks == 0);

   /* Ordinary cores may support loading without reporting a state size. */
   reset(100, false);
   assert(command_event_load_auto_state());
   assert(loads == 1 && size_checks == 0);

#ifdef HAVE_NETWORKING
   reset(1, true);
   assert(command_event_load_auto_state());
   allow_frame = false;
   core_run();
   runloop_load_deferred_auto_state();
   assert(runs == 0 && loads == 0 && size_checks == 0);
   assert(runloop_state.auto_state_load_pending);
   allow_frame = true;
   core_run();
   runloop_load_deferred_auto_state();
   assert(loads == 1);
#endif
#ifdef HAVE_CHEEVOS
   reset(1, true);
   hardcore = true;
   assert(!command_event_load_auto_state());
   assert(!runloop_state.auto_state_load_pending && loads == 0);
   hardcore = false;
#endif
   test_rolling_states();
   puts(rolling_failures ? "rolling-state failures" : "auto-state and rolling-state tests passed");
   return rolling_failures ? 1 : 0;
}

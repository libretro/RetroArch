/* Include the shipping loop to exercise its private deferred dispatcher. */
#include "../../../runloop.c"
#include <assert.h>

static settings_t test_settings;
static unsigned runs, size_checks, loads, settings_checks, path_checks;
static unsigned initialize_after;
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
#endif

settings_t *config_get_ptr(void)
{
   settings_checks++;
   return &test_settings;
}

void input_driver_poll(void) {}
void input_driver_poll_between_frames(void (*poll)(void)) { poll(); }
void video_driver_cached_frame(void) {}
void audio_driver_frame_end(void) {}
void audio_driver_publish_runloop(void) {}
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
bool verbosity_is_enabled(void) { return false; }

static void mock_run(void) { runs++; }
static size_t mock_size(void)
{
   size_checks++;
   return runs >= initialize_after ? 16 : 0;
}

bool content_load_state(const char *path, bool load_to_backup, bool autoload)
{
   assert(strcmp(path, "test.auto") == 0);
   assert(!load_to_backup && autoload);
   loads++;
   return true;
}

bool path_is_valid(const char *path)
{
   assert(strcmp(path, "test.auto") == 0);
   path_checks++;
   return true;
}

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   (void)msg;
   return "test";
}

static void reset(unsigned frames, bool quirk)
{
   memset(&runloop_state, 0, sizeof(runloop_state));
   memset(&test_settings, 0, sizeof(test_settings));
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
   puts("auto-state initialization tests passed");
   return 0;
}

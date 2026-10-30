/* Exercise the shipping storage and replay code with a synthetic core. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Qt defines this keyword before including the frontend headers. */
#define slots
#include "../../runloop.h"
#include "../../configuration.h"
#include "../../command.h"
#include "../../audio/audio_driver.h"
#include "../../gfx/video_driver.h"

typedef struct machine
{
   unsigned frame;
   unsigned enhanced[8];
} machine_t;

typedef struct snapshot
{
   machine_t machine;
   bool valid;
} snapshot_t;

static runloop_state_t runloop;
static preempt_t history;
static settings_t settings;
static audio_driver_state_t audio;
static video_driver_state_t video;
static struct retro_hw_render_callback hardware;
static machine_t live;
static unsigned created, released, saves, loads, polls, advances, pauses;
static unsigned context_depth, context_takes, context_releases;
static unsigned next_input, fail_save_at, fail_create_at;
static enum retro_snapshot_result save_failure, load_failure;
static bool serialized_load_failure, serialized_save_failure, retired;

static void fail(unsigned line, const char *expression)
{
   fprintf(stderr, "rollback_storage_test:%u: %s\n", line, expression);
   exit(1);
}

#define CHECK(x) do { if (!(x)) fail(__LINE__, #x); } while (0)
#define RARCH_ERR test_error
#define RARCH_LOG test_error

static void test_error(const char *message, ...)
{
   CHECK(message != NULL);
}

#ifdef HAVE_THREADS
void video_thread_core_context_take(void)
{
   CHECK(context_depth == 0);
   context_depth++;
   context_takes++;
}

void video_thread_core_context_release(void)
{
   CHECK(context_depth == 1);
   context_depth--;
   context_releases++;
}
#endif

static void check_context(void)
{
#ifdef HAVE_THREADS
   CHECK(context_depth == 1);
#else
   CHECK(context_depth == 0);
#endif
}

static bool supported(uint32_t count)
{
   CHECK(context_depth == 0);
   return count > 0 && count <= MAX_RUNAHEAD_FRAMES;
}

static enum retro_snapshot_result create(void **output)
{
   check_context();
   CHECK(*output == NULL);
   if (fail_create_at && created + 1 == fail_create_at)
      return RETRO_SNAPSHOT_UNAVAILABLE;
   *output = calloc(1, sizeof(snapshot_t));
   CHECK(*output != NULL);
   created++;
   return RETRO_SNAPSHOT_SUCCESS;
}

static enum retro_snapshot_result save(void *handle)
{
   snapshot_t *state = (snapshot_t *)handle;
   check_context();
   CHECK(runloop.flags & RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE);
   saves++;
   if (saves == fail_save_at)
      return save_failure;
   state->machine = live;
   state->valid = true;
   return RETRO_SNAPSHOT_SUCCESS;
}

static enum retro_snapshot_result load(const void *handle)
{
   const snapshot_t *state = (const snapshot_t *)handle;
   check_context();
   CHECK(runloop.flags & RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE);
   loads++;
   if (load_failure == RETRO_SNAPSHOT_FATAL)
      memset(&live, 0xee, sizeof(live));
   if (load_failure != RETRO_SNAPSHOT_SUCCESS)
      return load_failure;
   live = state->machine;
   return RETRO_SNAPSHOT_SUCCESS;
}

static void release(void *handle)
{
   check_context();
   released++;
   free(handle);
}

static bool valid(const void *handle)
{
   CHECK(context_depth == 0);
   return ((const snapshot_t *)handle)->valid;
}

size_t core_serialize_size_special(void)
{
   return sizeof(live);
}

static bool serialize(void *data, size_t size)
{
   check_context();
   CHECK(size == sizeof(live));
   if (serialized_save_failure)
      return false;
   memcpy(data, &live, size);
   return true;
}

static bool unserialize(const void *data, size_t size)
{
   check_context();
   CHECK(size == sizeof(live));
   if (serialized_load_failure)
   {
      memset(&live, 0xee, sizeof(live));
      return false;
   }
   memcpy(&live, data, size);
   return true;
}

runloop_state_t *runloop_state_get_ptr(void) { return &runloop; }
#include "snapshot_context_fragment.c"

static bool register_interface(void *data)
{
   runloop_state_t *runloop_st = &runloop;
   switch (RETRO_ENVIRONMENT_SET_SNAPSHOT_INTERFACE)
   {
#include "snapshot_registration_fragment.c"
   }
   return false;
}

#include "preempt_slab_fragment.c"
#include "rollback_storage_fragment.c"

settings_t *config_get_ptr(void) { return &settings; }
audio_driver_state_t *audio_state_get_ptr(void) { return &audio; }
video_driver_state_t *video_state_get_ptr(void) { return &video; }
struct retro_hw_render_callback *video_driver_get_hw_context(void) { return &hardware; }
static unsigned reset_calls, reset_releases, reset_invalidations, reset_retirements;
void video_driver_invalidate_hw_render_cache(void) { reset_invalidations++; }
void video_driver_cached_frame_retire(void) { reset_retirements++; }
void runahead_snapshot_release(void *data)
{
   CHECK(data == &runloop);
   CHECK(runloop.prediction_recovery == PREDICTION_RECOVERY_NONE);
   reset_releases++;
}
#define runloop_state runloop
#include "core_state_fragment.c"
#undef runloop_state

static unsigned memory_reads;
static size_t memory_size(unsigned id)
{
   CHECK(id == RETRO_MEMORY_SAVE_RAM);
   memory_reads++;
   return sizeof(live);
}

static void *memory_data(unsigned id)
{
   CHECK(id == RETRO_MEMORY_SAVE_RAM);
   memory_reads++;
   return &live;
}

static void core_state_cases(void)
{
   retro_ctx_memory_info_t info;
   memset(&runloop, 0, sizeof(runloop));
   memset(&hardware, 0, sizeof(hardware));
   retro_atomic_int_init(&video.flags, 0);
   runloop.current_core.retro_get_memory_data = memory_data;
   runloop.current_core.retro_get_memory_size = memory_size;
   memset(&info, 0, sizeof(info));
   info.id = RETRO_MEMORY_SAVE_RAM;
   CHECK(core_get_memory(&info) && info.data == &live && memory_reads == 2);
   hardware.context_type = RETRO_HW_CONTEXT_OPENGLES3;
   CHECK(core_state_is_available());
   retro_atomic_store_release_int(&video.flags, VIDEO_FLAG_GPU_DEVICE_LOST);
   CHECK(core_get_memory(&info) && info.data == NULL && info.size == 0);
   CHECK(memory_reads == 2);
   hardware.context_type = RETRO_HW_CONTEXT_NONE;
   CHECK(core_get_memory(&info) && info.data == &live && memory_reads == 4);
   runloop.prediction_recovery = PREDICTION_RECOVERY_FATAL;
   CHECK(core_get_memory(&info) && info.data == NULL && memory_reads == 4);
   retro_atomic_store_release_int(&video.flags, 0);
   CHECK(!core_state_is_available());
   runloop.prediction_recovery = PREDICTION_RECOVERY_NONE;
   CHECK(core_state_is_available());
}

static void reset_callback(void)
{
   check_context();
   reset_calls++;
}

static void reset_cases(void)
{
   enum prediction_recovery recovery;
   unsigned before;
   memset(&runloop, 0, sizeof(runloop));
   memset(&history, 0, sizeof(history));
   runloop.current_core.retro_reset = reset_callback;
   runloop.preempt_data = &history;
   for (recovery = PREDICTION_RECOVERY_NONE;
         recovery <= PREDICTION_RECOVERY_FATAL; recovery++)
   {
      runloop.prediction_recovery = recovery;
      history.frame_count = 3;
      reset_calls = reset_invalidations = reset_retirements = 0;
      before = reset_releases;
      core_reset();
      CHECK(reset_calls == 1);
      CHECK(reset_invalidations == reset_calls && reset_retirements == reset_calls);
      if (recovery != PREDICTION_RECOVERY_NONE)
      {
         CHECK(runloop.prediction_recovery == recovery);
         CHECK(!core_state_is_available());
         CHECK(reset_releases == before && history.frame_count == 3);
         core_reset();
         CHECK(reset_releases == before && !core_state_is_available());
         CHECK(reset_calls == 2 && reset_invalidations == 2 && reset_retirements == 2);
      }
      else
      {
         CHECK(runloop.prediction_recovery == PREDICTION_RECOVERY_NONE);
         CHECK(core_state_is_available());
         CHECK(reset_releases == before + 1 && history.frame_count == 0);
      }
      CHECK(context_depth == 0 && context_takes == context_releases);
   }
}
const char *msg_hash_to_str(enum msg_hash_enums message)
{
   (void)message;
   return "injected snapshot failure";
}

void runloop_msg_queue_push(const char *message, size_t length,
      unsigned priority, unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)message; (void)length; (void)priority; (void)duration;
   (void)flush; (void)title; (void)icon; (void)category;
}

bool command_event(enum event_command action, void *data)
{
   (void)data;
   CHECK(action == CMD_EVENT_PAUSE);
   pauses++;
   return true;
}

static void advance(machine_t *machine, unsigned input)
{
   machine->enhanced[machine->frame % 8] = 100 * input + machine->frame;
   machine->frame++;
}

static void runahead_core_run(runloop_state_t *state)
{
   CHECK(state->prediction_recovery == PREDICTION_RECOVERY_NONE);
   advances++;
   advance(&live, (unsigned)history.joypad_state[0]);
}

void audio_driver_frame_end(void) { }

static void preempt_input_poll(preempt_t *preempt,
      runloop_state_t *state, unsigned users)
{
   CHECK(users == 1);
   polls++;
   if (preempt->joypad_state[0] != (int16_t)next_input)
      state->flags |= RUNLOOP_FLAG_INPUT_IS_DIRTY;
   preempt->joypad_state[0] = (int16_t)next_input;
}

void preempt_deinit(void *data)
{
   runloop_state_t *state = (runloop_state_t *)data;
   CHECK(state->prediction_recovery == PREDICTION_RECOVERY_NONE);
   rollback_storage_release(state, &history.storage);
   state->preempt_data = NULL;
   state->snapshot_context = RETRO_SNAPSHOT_ORDINARY;
   retired = true;
}

#define PREEMPT_NEXT_PTR(x) ((x + 1) % preempt->frames)
#include "preempt_run_fragment.c"

static void reset(bool core_owned)
{
   memset(&runloop, 0, sizeof(runloop));
   memset(&history, 0, sizeof(history));
   memset(&live, 0, sizeof(live));
   created = released = saves = loads = polls = advances = pauses = 0;
   context_depth = context_takes = context_releases = 0;
   fail_save_at = fail_create_at = next_input = 0;
   save_failure = load_failure = RETRO_SNAPSHOT_SUCCESS;
   serialized_load_failure = serialized_save_failure = retired = false;
   settings.uints.input_max_users = 1;
   runloop.current_core.retro_serialize = serialize;
   runloop.current_core.retro_unserialize = unserialize;
   if (core_owned)
   {
      runloop.snapshot_iface.version = RETRO_SNAPSHOT_INTERFACE_VERSION;
      runloop.snapshot_iface.supported = supported;
      runloop.snapshot_iface.create = create;
      runloop.snapshot_iface.save = save;
      runloop.snapshot_iface.load = load;
      runloop.snapshot_iface.release = release;
      runloop.snapshot_iface.valid = valid;
   }
}

static void finish(rollback_storage_t *storage)
{
   rollback_storage_release(&runloop, storage);
   CHECK(created == released);
   CHECK(context_depth == 0 && context_takes == context_releases);
}

static void registration_cases(void)
{
   struct retro_snapshot_interface iface;
   struct
   {
      struct retro_snapshot_interface iface;
      unsigned char tail[16];
   } extended;
   uint32_t *prefix;
   unsigned i;
   reset(true);
   iface = runloop.snapshot_iface;
   iface.size = sizeof(iface);
   memset(&runloop.snapshot_iface, 0, sizeof(runloop.snapshot_iface));
   CHECK(!register_interface(NULL) && !runloop.snapshot_iface_declined);
   prefix = (uint32_t *)malloc(2 * sizeof(*prefix));
   CHECK(prefix != NULL);
   prefix[0] = 1;
   prefix[1] = 2 * sizeof(*prefix);
   CHECK(!register_interface(prefix) && runloop.snapshot_iface_declined);
   prefix[0] = RETRO_SNAPSHOT_INTERFACE_VERSION;
   CHECK(!register_interface(prefix));
   CHECK(prefix[0] == RETRO_SNAPSHOT_INTERFACE_VERSION);
   CHECK(prefix[1] == 2 * sizeof(*prefix));
   free(prefix);
   extended.iface = iface;
   extended.iface.size = sizeof(extended);
   memset(extended.tail, 0xa5, sizeof(extended.tail));
   CHECK(register_interface(&extended.iface));
   for (i = 0; i < sizeof(extended.tail); i++)
      CHECK(extended.tail[i] == 0xa5);
   CHECK(!runloop.snapshot_iface_declined);
   CHECK(runloop.snapshot_iface.size == sizeof(iface));
   CHECK(extended.iface.get_execution_context() == RETRO_SNAPSHOT_ORDINARY);
   runloop.snapshot_context = RETRO_SNAPSHOT_CANONICAL;
   CHECK(extended.iface.get_execution_context() == RETRO_SNAPSHOT_CANONICAL);
   runloop.snapshot_context = RETRO_SNAPSHOT_SPECULATIVE;
   CHECK(extended.iface.get_execution_context() == RETRO_SNAPSHOT_SPECULATIVE);
   CHECK(!register_interface(&iface) && !runloop.snapshot_iface_declined);
   CHECK(created == 0 && saves == 0 && loads == 0 && advances == 0);
}

static void storage_cases(bool core_owned)
{
   rollback_storage_t storage;
   machine_t expected;
   reset(core_owned);
   memset(&storage, 0, sizeof(storage));
   CHECK(rollback_storage_init(&runloop, &storage, 2) == RETRO_SNAPSHOT_SUCCESS);
   advance(&live, 1);
   expected = live;
   CHECK(rollback_storage_transfer(&runloop, &storage, 0, false) == RETRO_SNAPSHOT_SUCCESS);
   advance(&live, 2);
   CHECK(rollback_storage_transfer(&runloop, &storage, 1, false) == RETRO_SNAPSHOT_SUCCESS);
   CHECK(rollback_storage_transfer(&runloop, &storage, 0, true) == RETRO_SNAPSHOT_SUCCESS);
   CHECK(memcmp(&live, &expected, sizeof(live)) == 0);
   advance(&live, 3);
   CHECK(rollback_storage_transfer(&runloop, &storage, 0, true) == RETRO_SNAPSHOT_SUCCESS);
   CHECK(memcmp(&live, &expected, sizeof(live)) == 0);
   runloop.flags |= RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;
   CHECK(rollback_storage_transfer(&runloop, &storage, 1, true) == RETRO_SNAPSHOT_SUCCESS);
   CHECK(runloop.flags & RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE);
   if (core_owned)
   {
      fail_save_at = saves + 1;
      save_failure = RETRO_SNAPSHOT_UNAVAILABLE;
      CHECK(rollback_storage_transfer(&runloop, &storage, 0, false) == RETRO_SNAPSHOT_UNAVAILABLE);
      CHECK(rollback_storage_transfer(&runloop, &storage, 0, true) == RETRO_SNAPSHOT_SUCCESS);
      CHECK(memcmp(&live, &expected, sizeof(live)) == 0);
   }
   else
   {
      serialized_load_failure = true;
      CHECK(rollback_storage_transfer(&runloop, &storage, 0, true) == RETRO_SNAPSHOT_FATAL);
   }
   finish(&storage);
}

static void setup_history(void)
{
   unsigned i;
   reset(true);
   history.frames = 3;
   runloop.preempt_data = &history;
   CHECK(rollback_storage_init(&runloop, &history.storage, 3) == RETRO_SNAPSHOT_SUCCESS);
   for (i = 0; i < 3; i++)
   {
      CHECK(rollback_storage_transfer(&runloop, &history.storage, i, false) == RETRO_SNAPSHOT_SUCCESS);
      advance(&live, 0);
   }
   history.frame_count = 3;
   next_input = 1;
}

static void initial_capture_cases(void)
{
   machine_t expected;
   unsigned i;
   reset(true);
   history.frames = 3;
   runloop.preempt_data = &history;
   CHECK(rollback_storage_init(&runloop, &history.storage, 3) == RETRO_SNAPSHOT_SUCCESS);
   fail_save_at = 1;
   save_failure = RETRO_SNAPSHOT_UNAVAILABLE;
   next_input = 1;
   CHECK(preempt_run(&history, &runloop));
   CHECK(!retired && advances == 1 && loads == 0);
   CHECK(history.frame_count == 0 && history.start_ptr == 0);
   CHECK(!valid(history.storage.states[0]));
   next_input = 2;
   for (i = 0; i < 3; i++)
      CHECK(preempt_run(&history, &runloop));
   CHECK(history.frame_count == 3 && advances == 4 && loads == 0);
   next_input = 3;
   CHECK(preempt_run(&history, &runloop));
   memset(&expected, 0, sizeof(expected));
   advance(&expected, 1);
   for (i = 0; i < 4; i++)
      advance(&expected, 3);
   CHECK(loads == 1 && memcmp(&live, &expected, sizeof(live)) == 0);
   finish(&history.storage);

   reset(true);
   history.frames = 3;
   runloop.preempt_data = &history;
   CHECK(rollback_storage_init(&runloop, &history.storage, 3) == RETRO_SNAPSHOT_SUCCESS);
   fail_save_at = 1;
   save_failure = RETRO_SNAPSHOT_UNAVAILABLE;
   CHECK(preempt_run(&history, &runloop) && !retired);
   fail_save_at = 2;
   CHECK(preempt_run(&history, &runloop));
   CHECK(retired && advances == 2 && saves == 2 && created == released);

   reset(true);
   history.frames = 3;
   runloop.preempt_data = &history;
   CHECK(rollback_storage_init(&runloop, &history.storage, 3) == RETRO_SNAPSHOT_SUCCESS);
   for (i = 0; i < 3; i++)
      CHECK(preempt_run(&history, &runloop));
   CHECK(history.frame_count == 3 && advances == 3);
   ((snapshot_t *)history.storage.states[history.start_ptr])->valid = false;
   fail_save_at = saves + 1;
   save_failure = RETRO_SNAPSHOT_UNAVAILABLE;
   CHECK(preempt_run(&history, &runloop));
   CHECK(retired && advances == 4 && history.frame_count == 0 && created == released);

   reset(false);
   history.frames = 3;
   runloop.preempt_data = &history;
   CHECK(rollback_storage_init(&runloop, &history.storage, 3) == RETRO_SNAPSHOT_SUCCESS);
   serialized_save_failure = true;
   CHECK(preempt_run(&history, &runloop));
   CHECK(retired && advances == 1);

   reset(true);
   history.frames = 3;
   runloop.preempt_data = &history;
   CHECK(rollback_storage_init(&runloop, &history.storage, 3) == RETRO_SNAPSHOT_SUCCESS);
   fail_save_at = 1;
   save_failure = RETRO_SNAPSHOT_FATAL;
   CHECK(!preempt_run(&history, &runloop));
   CHECK(advances == 0 && pauses == 1 && !retired);
   CHECK(runloop.prediction_recovery == PREDICTION_RECOVERY_FATAL);
   finish(&history.storage);
}

static void replay_cases(enum retro_snapshot_result failure, bool fail_load)
{
   machine_t expected;
   unsigned i, before;
   setup_history();
   memset(&expected, 0, sizeof(expected));
   for (i = 0; i < 4; i++)
      advance(&expected, 1);
   if (fail_load)
      load_failure = failure;
   else
   {
      fail_save_at = saves + 1;
      save_failure = failure;
   }
   if (failure == RETRO_SNAPSHOT_FATAL)
   {
      CHECK(!preempt_run(&history, &runloop));
      CHECK(runloop.prediction_recovery == PREDICTION_RECOVERY_PREEMPT);
      CHECK(pauses == 1 && !retired && history.start_ptr == 0);
      CHECK(valid(history.storage.states[0]));
      CHECK(advances == (fail_load ? 0u : 1u));
      before = polls;
      next_input = 2;
      load_failure = RETRO_SNAPSHOT_SUCCESS;
      fail_save_at = 0;
      CHECK(preempt_run(&history, &runloop));
      CHECK(polls == before && next_input == 2);
   }
   else
      CHECK(preempt_run(&history, &runloop));
   CHECK(memcmp(&live, &expected, sizeof(live)) == 0);
   CHECK(retired && runloop.prediction_recovery == PREDICTION_RECOVERY_NONE);
   CHECK(created == released && context_takes == context_releases);
}

/* Exercise the cached SRAM pointer through the shipping snapshot consumer. */
#ifdef HAVE_THREADS
typedef struct test_autosave
{
   void *snapshot;
   const void *retro_buffer;
   size_t bufsize;
   retro_atomic_int_t snapshot_requested;
   unsigned ec;
} test_autosave_t;

static struct
{
   test_autosave_t **list;
   unsigned num, depth;
} autosave_state;

static void test_snapshot_notify(unsigned *event) { (*event)++; }
#define autosave_t test_autosave_t
#define retro_eventcount_notify test_snapshot_notify
#include "autosave_check_fragment.c"
#undef retro_eventcount_notify
#undef autosave_t

static void autosave_cases(void)
{
   unsigned live_sram = 1, copied_sram = 0;
   test_autosave_t save;
   test_autosave_t *list[1];
   enum prediction_recovery recovery;
   memset(&save, 0, sizeof(save));
   list[0] = &save;
   autosave_state.list = list;
   autosave_state.num = 1;
   save.snapshot = &copied_sram;
   save.retro_buffer = &live_sram;
   save.bufsize = sizeof(live_sram);
   retro_atomic_int_init(&save.snapshot_requested, 1);
   runloop.prediction_recovery = PREDICTION_RECOVERY_NONE;
   autosave_check();
   CHECK(copied_sram == 1 && save.ec == 1);
   live_sram = 99;
   retro_atomic_store_release_int(&save.snapshot_requested, 1);
   for (recovery = PREDICTION_RECOVERY_RUNAHEAD;
         recovery <= PREDICTION_RECOVERY_FATAL; recovery++)
   {
      runloop.prediction_recovery = recovery;
      autosave_check();
      autosave_check();
      CHECK(copied_sram == 1 && save.ec == 1);
      CHECK(retro_atomic_load_acquire_int(&save.snapshot_requested) == 1);
   }
   runloop.prediction_recovery = PREDICTION_RECOVERY_NONE;
   hardware.context_type = RETRO_HW_CONTEXT_OPENGLES3;
   retro_atomic_store_release_int(&video.flags, VIDEO_FLAG_GPU_DEVICE_LOST);
   autosave_check();
   CHECK(copied_sram == 1 && save.ec == 1);
   CHECK(retro_atomic_load_acquire_int(&save.snapshot_requested) == 1);
   retro_atomic_store_release_int(&video.flags, 0);
   hardware.context_type = RETRO_HW_CONTEXT_NONE;
   autosave_state.depth = 1;
   autosave_check();
   CHECK(copied_sram == 1 && save.ec == 1);
   live_sram = 2;
   autosave_state.depth = 0;
   autosave_check();
   CHECK(copied_sram == 2 && save.ec == 2);
   CHECK(retro_atomic_load_acquire_int(&save.snapshot_requested) == 0);
   live_sram = 3;
   autosave_check();
   CHECK(copied_sram == 2 && save.ec == 2);
   memset(&autosave_state, 0, sizeof(autosave_state));
}
#endif

int main(void)
{
   rollback_storage_t storage;
   core_state_cases();
   reset_cases();
   registration_cases();
#ifdef HAVE_THREADS
   autosave_cases();
#endif
   storage_cases(false);
   storage_cases(true);
   initial_capture_cases();
   reset(true);
   memset(&storage, 0, sizeof(storage));
   fail_create_at = 2;
   CHECK(rollback_storage_init(&runloop, &storage, 3) == RETRO_SNAPSHOT_UNAVAILABLE);
   CHECK(storage.states == NULL && created == released);
   runloop.snapshot_iface_declined = true;
   CHECK(rollback_storage_init(&runloop, &storage, 1) == RETRO_SNAPSHOT_UNAVAILABLE);
   CHECK(created == 1);
   replay_cases(RETRO_SNAPSHOT_UNAVAILABLE, false);
   replay_cases(RETRO_SNAPSHOT_FATAL, false);
   replay_cases(RETRO_SNAPSHOT_FATAL, true);
   puts("rollback storage and replay: passed");
   return 0;
}

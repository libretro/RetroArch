/* Include the shipping owner implementation, including its private save path. */
#undef NDEBUG
#include "../../../runahead.c"
#include <assert.h>
#include "reference_core.h"

static runloop_state_t state;
static unsigned messages, pauses, context_depth;
static unsigned char buffer[32];

runloop_state_t *runloop_state_get_ptr(void) { return &state; }
size_t core_serialize_size_special(void) { return retro_serialize_size(); }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   assert(msg == MSG_CORE_STATE_UNRECOVERABLE);
   return "reload content";
}

void runloop_msg_queue_push(const char *msg, size_t len, unsigned priority,
      unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)len;
   (void)priority;
   (void)duration;
   (void)flush;
   (void)title;
   (void)icon;
   assert(strcmp(msg, "reload content") == 0);
   assert(category == MESSAGE_QUEUE_CATEGORY_ERROR);
   messages++;
}

bool command_event(enum event_command command, void *data)
{
   (void)data;
   assert(command == CMD_EVENT_PAUSE);
   pauses++;
   return true;
}

#ifdef HAVE_THREADS
void video_thread_core_context_take(void) { assert(!context_depth); context_depth++; }
void video_thread_core_context_release(void) { assert(context_depth == 1); context_depth--; }
#endif

static bool environment(unsigned command, void *data)
{
   switch (command)
   {
      case RETRO_ENVIRONMENT_SET_SAVESTATE_LIFETIME:
         return serialization_register((const struct retro_savestate_lifetime*)data);
      case RETRO_ENVIRONMENT_GET_SAVESTATE_PHASE:
         if (data)
            *(enum retro_savestate_phase*)data = runahead_get_savestate_phase();
         return true;
      case RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT:
         *(int*)data = serialization_owned_context()
            ? RETRO_SAVESTATE_CONTEXT_RUNAHEAD_MANAGED : RETRO_SAVESTATE_CONTEXT_NORMAL;
         return true;
      default:
         return false;
   }
}

static void start(void)
{
   memset(&state, 0, sizeof(state));
   state.current_core.retro_serialize = retro_serialize;
   state.current_core.retro_unserialize = retro_unserialize;
   state.current_core.retro_run = retro_run;
   state.runahead_savestate_info.data = buffer;
   state.runahead_savestate_info.data_const = buffer;
   state.runahead_savestate_info.size = sizeof(buffer);
   messages = pauses = context_depth = 0;
   retro_set_environment(environment);
   retro_init();
   assert(serialization_size() == sizeof(buffer));
}

static void finish(void)
{
   serialization_close();
   retro_unload_game();
   retro_deinit();
   assert(!context_depth);
}

static void release_once(void)
{
   unsigned char copy[sizeof(buffer)];
   unsigned char expected[sizeof(buffer)], restored[sizeof(buffer)];
   size_t size;
   start();
   size = retro_serialize_size();
   assert(size <= sizeof(expected) && retro_serialize(expected, size));
   assert(serialization_save(buffer, sizeof(buffer), &state.runahead_state_owned));
   memcpy(copy, buffer, sizeof(copy));
   serialization_run(&state.current_core);
   assert(retro_serialize(restored, size) && memcmp(expected, restored, size) != 0);
   assert(serialization_load(copy, sizeof(copy)));
   assert(retro_serialize(restored, size) && memcmp(expected, restored, size) == 0);
   assert(serialization_load(copy, sizeof(copy)));
   serialization_clear_history();
   serialization_clear_history();
   assert(lifetime_test_counts()->saves == 1);
   assert(lifetime_test_counts()->releases == 1);
   finish();
}

static void failed_save_preserves_state(void)
{
   unsigned char copy[sizeof(buffer)];
   start();
   assert(serialization_save(buffer, sizeof(buffer), &state.runahead_state_owned));
   memcpy(copy, buffer, sizeof(copy));
   lifetime_test_refuse_save();
   assert(!serialization_save(buffer, sizeof(buffer), &state.runahead_state_owned));
   assert(memcmp(copy, buffer, sizeof(copy)) == 0);
   assert(state.runahead_state_owned && lifetime_test_counts()->live == 1);
   assert(lifetime_test_counts()->releases == 0);
   assert(serialization_load(buffer, sizeof(buffer)));
   finish();
}

static void unusable_stops_and_reports(void)
{
   start();
   lifetime_test_set_usable(false);
   assert(!serialization_check());
   assert(!serialization_check());
   serialization_run(&state.current_core);
   assert(!lifetime_test_counts()->runs && messages == 1 && pauses == 1);
   finish();
}

static void registration_after_init(void)
{
   struct retro_savestate_lifetime callbacks;
   start();
   callbacks = serialization_interface;
   serialization_close();
   state.current_core.flags = RETRO_CORE_FLAG_INITED;
   assert(!serialization_register(&callbacks));
   finish();
}

static preempt_t *populated_history(void)
{
   unsigned i;
   preempt_t *preempt;
   preempt = (preempt_t*)calloc(1, sizeof(*preempt));
   assert(preempt);
   preempt->frames = 3;
   preempt->state_size = sizeof(buffer);
   assert(preempt_slab_alloc(preempt->buffer, preempt->frames, preempt->state_size));
   state.preempt_data = preempt;
   for (i = 0; i < preempt->frames; i++)
      assert(serialization_save(preempt->buffer[i], preempt->state_size,
               &preempt->state_owned[i]));
   preempt->frame_count = preempt->frames;
   preempt->start_ptr = preempt->replay_ptr = 1;
   return preempt;
}

static void preempt_deinit_releases_states(void)
{
   start();
   populated_history();
   preempt_deinit(&state);
   preempt_deinit(&state);
   assert(!state.preempt_data && lifetime_test_counts()->releases == 3);
   finish();
}

static void history_clear_resets_replay_window(void)
{
   preempt_t *preempt;
   start();
   preempt = populated_history();
   serialization_clear_history();
   assert(!preempt->frame_count && !preempt->start_ptr && !preempt->replay_ptr);
   assert(!lifetime_test_counts()->live && lifetime_test_counts()->releases == 3);
   preempt_deinit(&state);
   assert(lifetime_test_counts()->releases == 3);
   finish();
}

int main(void)
{
#define RUN(test) do { test(); puts("PASS " #test); } while (0)
   RUN(release_once);
   RUN(failed_save_preserves_state);
   RUN(unusable_stops_and_reports);
   RUN(registration_after_init);
   RUN(preempt_deinit_releases_states);
   RUN(history_clear_resets_replay_window);
   return 0;
}

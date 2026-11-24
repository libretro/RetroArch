/* Include the shipping owner implementation, including its private save path. */
#undef NDEBUG
#include "../../../runahead.c"
#include <assert.h>
#include "reference_core.h"

static runloop_state_t state;
static unsigned context_depth;
static unsigned char buffer[32];

runloop_state_t *runloop_state_get_ptr(void) { return &state; }
size_t core_serialize_size_special(void) { return retro_serialize_size(); }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }

/* The rest of runahead.c links against these; no test reaches them. */
#define NOT_REACHED() (fprintf(stderr, "not reached: %s\n", __func__), abort())
void audio_driver_frame_end(void) { NOT_REACHED(); }
audio_driver_state_t *audio_state_get_ptr(void) { NOT_REACHED(); return NULL; }
settings_t *config_get_ptr(void) { NOT_REACHED(); return NULL; }
bool core_info_current_supports_runahead(void) { NOT_REACHED(); return false; }
bool core_run(void) { NOT_REACHED(); return false; }
bool core_serialize_special(retro_ctx_serialize_info_t *info) { (void)info; NOT_REACHED(); return false; }
bool core_unserialize_special(retro_ctx_serialize_info_t *info) { (void)info; NOT_REACHED(); return false; }
void input_driver_poll(void) { NOT_REACHED(); }
int16_t input_driver_state_wrapper(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{ (void)port; (void)device; (void)idx; (void)id; NOT_REACHED(); return 0; }
void retro_input_poll_null(void) { NOT_REACHED(); }
struct string_list *string_list_clone(const struct string_list *src) { (void)src; NOT_REACHED(); return NULL; }
void string_list_free(struct string_list *list) { (void)list; NOT_REACHED(); }
void video_driver_modify_disp_flags(uint32_t set_bits, uint32_t clear_bits)
{ (void)set_bits; (void)clear_bits; NOT_REACHED(); }
video_driver_state_t *video_state_get_ptr(void) { NOT_REACHED(); return NULL; }

const char *msg_hash_to_str(enum msg_hash_enums msg) { (void)msg; NOT_REACHED(); return ""; }
void runloop_msg_queue_push(const char *msg, size_t len, unsigned priority,
      unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)msg; (void)len; (void)priority; (void)duration; (void)flush;
   (void)title; (void)icon; (void)category; NOT_REACHED();
}
void core_reset(void) { NOT_REACHED(); }

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
      case RETRO_ENVIRONMENT_GET_EXECUTION_PHASE:
         if (data)
            *(enum retro_execution_phase*)data = runahead_get_execution_phase();
         return true;
      case RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT:
         *(int*)data = serialization_owned_context()
            ? RETRO_SAVESTATE_CONTEXT_SAME_INSTANCE_MANAGED : RETRO_SAVESTATE_CONTEXT_NORMAL;
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
   context_depth = 0;
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

static void save_in_place(void)
{
   uint64_t id;
   start();
   assert(serialization_save(buffer, sizeof(buffer), &state.runahead_state_owned));
   assert(serialization_save(buffer, sizeof(buffer), &state.runahead_state_owned));
   memcpy(&id, buffer, sizeof(id));
   assert(id == 2 && state.runahead_state_owned);
   assert(lifetime_test_counts()->releases == 1);
   assert(lifetime_test_counts()->max_live == 1);
   finish();
}

static void refused_save_owns_nothing(void)
{
   start();
   assert(serialization_save(buffer, sizeof(buffer), &state.runahead_state_owned));
   lifetime_test_refuse_save();
   assert(!serialization_save(buffer, sizeof(buffer), &state.runahead_state_owned));
   assert(!state.runahead_state_owned && !lifetime_test_counts()->live);
   assert(lifetime_test_counts()->releases == 1);
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
   RUN(save_in_place);
   RUN(refused_save_owns_nothing);
   RUN(registration_after_init);
   RUN(preempt_deinit_releases_states);
   RUN(history_clear_resets_replay_window);
   return 0;
}

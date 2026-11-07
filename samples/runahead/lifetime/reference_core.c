/* CPU resources stand in for core-owned GPU images; no graphics driver needed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "reference_core.h"

struct machine_state
{
   unsigned magic, frame, buttons, epoch;
};

struct retained_state
{
   struct machine_state machine;
   bool live;
};

static struct machine_state machine;
static struct retained_state states[4096];
static struct lifetime_counts counts;
static retro_environment_t environment;
static retro_video_refresh_t video;
static retro_input_poll_t poll_input;
static retro_input_state_t input;
static const char *test_case;
static FILE *trace;
static unsigned serial;
static bool usable, injected, refuse_save;

static enum retro_savestate_phase phase(void)
{
   enum retro_savestate_phase value = RETRO_SAVESTATE_PHASE_NORMAL;
   environment(RETRO_ENVIRONMENT_GET_SAVESTATE_PHASE, &value);
   return value;
}

static void event(const char *name, unsigned value)
{
   if (trace)
   {
      fprintf(trace, "%s %p %u %u %d\n", name, (void*)&machine,
            value, machine.frame, (int)phase());
      fflush(trace);
   }
}

#define CHECK(condition) do { if (!(condition)) { \
   event("FAIL", __LINE__); \
   fprintf(stderr, "lifetime reference core: line %u: %s\n", \
         (unsigned)__LINE__, #condition); exit(1); } } while (0)

static bool is_case(const char *name)
{
   return strcmp(test_case, name) == 0;
}

static bool managed(void)
{
   enum retro_savestate_context context = RETRO_SAVESTATE_CONTEXT_NORMAL;
   environment(RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT, &context);
   return context == RETRO_SAVESTATE_CONTEXT_RUNAHEAD_MANAGED;
}

static bool RETRO_CALLCONV is_usable(void)
{
   return usable;
}

static unsigned token(const void *data, size_t size)
{
   uint64_t id;
   CHECK(size >= sizeof(id));
   memcpy(&id, data, sizeof(id));
   CHECK(id && id <= serial && states[id].live);
   return (unsigned)id;
}

static void RETRO_CALLCONV release(const void *data, size_t size)
{
   unsigned id = token(data, size);
   states[id].live = false;
   counts.releases++;
   counts.live--;
   event("RELEASE", id);
}

const struct lifetime_counts *lifetime_test_counts(void) { return &counts; }
void lifetime_test_refuse_save(void) { refuse_save = true; }
void lifetime_test_set_usable(bool value) { usable = value; }

void retro_set_environment(retro_environment_t cb)
{
   static const struct retro_variable options[] = {
      { "lifetime_test_option", "Test option; unchanged|changed" },
      { NULL, NULL }
   };
   environment = cb;
   environment(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)options);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { (void)cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_input = cb; }
void retro_set_input_state(retro_input_state_t cb) { input = cb; }
void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
   CHECK(usable);
}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name = "Savestate lifetime reference";
   info->library_version = "1";
   info->valid_extensions = "lifetime";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width = info->geometry.max_width = 16;
   info->geometry.base_height = info->geometry.max_height = 16;
   info->timing.fps = 60.0;
   info->timing.sample_rate = 48000.0;
}

void retro_init(void)
{
   static struct retro_memory_descriptor descriptor;
   struct retro_memory_map map;
   struct retro_savestate_lifetime callbacks = {
      RETRO_SAVESTATE_LIFETIME_VERSION, sizeof(callbacks), release, is_usable
   };
   const char *path = getenv("LIFETIME_TEST_TRACE");
   bool registered = false;
   test_case = getenv("LIFETIME_TEST_CASE");
   if (!test_case)
      test_case = "release_once";
   if (path)
   {
      trace = fopen(path, "a");
      CHECK(trace);
   }
   memset(&machine, 0, sizeof(machine));
   memset(states, 0, sizeof(states));
   memset(&counts, 0, sizeof(counts));
   machine.magic = 0x53544154;
   machine.epoch = 1;
   serial = 0;
   usable = true;
   injected = refuse_save = false;
   memset(&descriptor, 0, sizeof(descriptor));
   descriptor.ptr = &machine;
   descriptor.len = sizeof(machine);
   map.descriptors = &descriptor;
   map.num_descriptors = 1;
   environment(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &map);
   if (is_case("old_version"))
      callbacks.version = 1;
   if (!is_case("late_registration") && !is_case("unregistered"))
      registered = environment(RETRO_ENVIRONMENT_SET_SAVESTATE_LIFETIME, &callbacks);
   CHECK(registered == !(is_case("late_registration") ||
            is_case("unregistered") || is_case("old_version")));
   event("INIT", registered);
}

bool retro_load_game(const struct retro_game_info *info) { return info != NULL; }
bool retro_load_game_special(unsigned id, const struct retro_game_info *info, size_t count)
{
   (void)id;
   (void)info;
   (void)count;
   return false;
}

void retro_unload_game(void)
{
   CHECK(!counts.live && counts.saves == counts.releases);
   event("UNLOAD", counts.releases);
}

void retro_deinit(void)
{
   CHECK(!counts.live && counts.saves == counts.releases);
   event("DEINIT", counts.releases);
   if (trace)
      fclose(trace);
   trace = NULL;
}

void retro_reset(void)
{
   CHECK(usable && !counts.live);
   machine.frame = 0;
   machine.epoch++;
   event("RESET", 0);
}

void retro_cheat_reset(void) { CHECK(usable); }
void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
   CHECK(usable);
}

void *retro_get_memory_data(unsigned id)
{
   CHECK(usable);
   return id == RETRO_MEMORY_SYSTEM_RAM || id == RETRO_MEMORY_SAVE_RAM ? &machine : NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   CHECK(usable);
   return id == RETRO_MEMORY_SYSTEM_RAM || id == RETRO_MEMORY_SAVE_RAM ? sizeof(machine) : 0;
}

size_t retro_serialize_size(void)
{
   CHECK(usable);
   return managed() ? 32 : sizeof(machine);
}

bool retro_serialize(void *data, size_t size)
{
   CHECK(usable);
   if (managed())
   {
      uint64_t id;
      CHECK(size == 32);
      if (refuse_save || (!injected &&
               ((is_case("failed_save") && counts.saves >= 12) ||
                (is_case("preempt_save_error") && phase() == RETRO_SAVESTATE_PHASE_SPECULATIVE))))
      {
         injected = true;
         refuse_save = false;
         memset(data, 0xff, size);
         event("REFUSE_SAVE", serial);
         return false;
      }
      id = ++serial;
      CHECK(id < sizeof(states) / sizeof(states[0]));
      states[id].machine = machine;
      states[id].live = true;
      counts.saves++;
      counts.live++;
      memcpy(data, &id, sizeof(id));
      event("SAVE", (unsigned)id);
      return true;
   }
   CHECK(size == sizeof(machine));
   memcpy(data, &machine, size);
   event("ORDINARY_SAVE", 0);
   return true;
}

bool retro_unserialize(const void *data, size_t size)
{
   CHECK(usable);
   if (managed())
   {
      unsigned id = token(data, size);
      CHECK(states[id].machine.epoch == machine.epoch);
      if (!injected && is_case("preempt_load_error") && counts.loads >= 12)
      {
         injected = true;
         event("REFUSE_LOAD", id);
         return false;
      }
      machine = states[id].machine;
      counts.loads++;
      event("LOAD", id);
      return true;
   }
   CHECK(size == sizeof(machine));
   memcpy(&machine, data, size);
   CHECK(machine.magic == 0x53544154);
   event("ORDINARY_LOAD", 0);
   return true;
}

void retro_run(void)
{
   static const uint16_t pixels[16 * 16];
   bool updated = false;
   CHECK(usable);
   if (poll_input)
      poll_input();
   if (input)
      machine.buttons = (unsigned)input(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
   machine.frame++;
   counts.runs++;
   event("RUN", counts.runs);
   if (!injected && counts.runs >= 45 && phase() == RETRO_SAVESTATE_PHASE_CANONICAL)
   {
      if (is_case("halt"))
      {
         injected = true;
         usable = false;
         event("UNSAFE", counts.runs);
      }
      else if (is_case("late_registration"))
      {
         struct retro_savestate_lifetime callbacks = {
            RETRO_SAVESTATE_LIFETIME_VERSION, sizeof(callbacks), release, is_usable
         };
         injected = true;
         CHECK(!environment(RETRO_ENVIRONMENT_SET_SAVESTATE_LIFETIME, &callbacks));
         event("LATE_REJECTED", 0);
      }
      else if (is_case("options_pending"))
      {
         struct retro_variable option = { "lifetime_test_option", "changed" };
         injected = true;
         CHECK(environment(RETRO_ENVIRONMENT_SET_VARIABLE, &option));
         event("OPTION", serial);
      }
   }
   environment(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated);
   if (updated && (!is_case("options_pending") || !injected || counts.runs >= 75))
   {
      struct retro_variable option = { "lifetime_test_option", NULL };
      environment(RETRO_ENVIRONMENT_GET_VARIABLE, &option);
      if (is_case("options_pending") && injected)
         event("OPTIONS_READ", counts.runs);
   }
   if (video)
      video(pixels, 16, 16, 16 * sizeof(*pixels));
}

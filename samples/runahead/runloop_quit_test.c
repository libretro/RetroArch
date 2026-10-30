/* Exercise the shipping quit check, including input-release debounce. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int64_t retro_time_t;
#define INP_FLAG_WAIT_INPUT_RELEASE 1
#define RUNLOOP_FLAG_SHUTDOWN_INITIATED 1
#define RUNLOOP_FLAG_CORE_SHUTDOWN_INITIATED 2
#define RUNLOOP_FLAG_CORE_RUNNING 4
#define RUNLOOP_STATE_QUIT 1
#define RARCH_QUIT_KEY 0
#define BIT256_GET(bits, key) (bits)
#define BSV_MOVIE_IS_EOF() || movie_eof
#define QUIT_DELAY_USEC 1000000
#define MSG_PRESS_AGAIN_TO_QUIT 0
#define RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE 0
#define MESSAGE_QUEUE_ICON_DEFAULT 0
#define MESSAGE_QUEUE_CATEGORY_WARNING 0
#define CMD_EVENT_UNLOAD_CORE 0

static struct { unsigned flags, max_frames; } runloop_state;
static struct { unsigned flags; } input_state;
static struct { struct { bool confirm_quit, load_dummy_on_core_shutdown; } bools; } config;
static unsigned frame_count, unloads, messages;
static bool is_alive = true, movie_eof, unload_ok = true, loaded_dummy;
static retro_time_t current_time = 10000000;

static const char *msg_hash_to_str(unsigned id) { (void)id; return "Quit"; }
static bool runloop_environment_cb(unsigned cmd, void *data)
{ (void)cmd; *(float*)data = 60.0f; return true; }
static void runloop_msg_queue_push(const char *s, size_t n, unsigned p,
      unsigned frames, bool flush, void *userdata, unsigned icon, unsigned category)
{
   (void)s; (void)n; (void)p; (void)frames; (void)flush;
   (void)userdata; (void)icon; (void)category; messages++;
}
static bool command_event(unsigned cmd, void *data)
{ (void)cmd; unloads++; loaded_dummy = *(bool*)data; return unload_ok; }

#include "runloop_exit_macro.h"

static int check_quit(bool current_bits)
{
   __typeof__(runloop_state) *runloop_st = &runloop_state;
   __typeof__(input_state) *input_st = &input_state;
   __typeof__(config) *settings = &config;
#include "runloop_quit_fragment.c"
   return 0;
}

int main(void)
{
   input_state.flags = INP_FLAG_WAIT_INPUT_RELEASE;
   assert(!check_quit(true)); /* Debounced hotkey must stay ignored. */
   runloop_state.flags = RUNLOOP_FLAG_SHUTDOWN_INITIATED |
         RUNLOOP_FLAG_CORE_SHUTDOWN_INITIATED | RUNLOOP_FLAG_CORE_RUNNING;
   assert(check_quit(true) == RUNLOOP_STATE_QUIT);
   assert(unloads == 1 && !loaded_dummy);
   assert(!(runloop_state.flags & RUNLOOP_FLAG_CORE_RUNNING));

   config.bools.load_dummy_on_core_shutdown = true;
   runloop_state.flags = RUNLOOP_FLAG_SHUTDOWN_INITIATED |
         RUNLOOP_FLAG_CORE_SHUTDOWN_INITIATED;
   assert(!check_quit(false));
   assert(unloads == 2 && loaded_dummy && !runloop_state.flags);
   unload_ok = false;
   runloop_state.flags = RUNLOOP_FLAG_SHUTDOWN_INITIATED |
         RUNLOOP_FLAG_CORE_SHUTDOWN_INITIATED;
   assert(check_quit(false) == RUNLOOP_STATE_QUIT);
   runloop_state.flags = 0;
   unload_ok = true;

   is_alive = false;
   assert(check_quit(false) == RUNLOOP_STATE_QUIT);
   is_alive = true;
   movie_eof = true;
   assert(check_quit(false) == RUNLOOP_STATE_QUIT);
   movie_eof = false;
   runloop_state.max_frames = frame_count = 10;
   assert(check_quit(false) == RUNLOOP_STATE_QUIT);
   runloop_state.max_frames = frame_count = 0;
   assert(!check_quit(false));

   input_state.flags = 0;
   assert(!check_quit(false));
   assert(check_quit(true) == RUNLOOP_STATE_QUIT);
   assert(!check_quit(true)); /* A held key is one press. */
   input_state.flags = INP_FLAG_WAIT_INPUT_RELEASE;
   assert(!check_quit(false));
   input_state.flags = 0;
   assert(!check_quit(true)); /* Debounce must preserve the held-key state. */
   assert(!check_quit(false));
   config.bools.confirm_quit = true;
   assert(!check_quit(true) && messages == 1);
   assert(!check_quit(true) && messages == 1);
   assert(!check_quit(false));
   current_time += 100000;
   assert(check_quit(true) == RUNLOOP_STATE_QUIT);
   assert(!check_quit(false));
   current_time += 2 * QUIT_DELAY_USEC;
   assert(!check_quit(true) && messages == 2);
   runloop_state.flags = RUNLOOP_FLAG_SHUTDOWN_INITIATED;
   input_state.flags = INP_FLAG_WAIT_INPUT_RELEASE;
   assert(check_quit(true) == RUNLOOP_STATE_QUIT);
   puts("runloop_quit_test: passed");
   return 0;
}

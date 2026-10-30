/* The worker exits between block's alive load and its acknowledgement reset. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>

static retro_atomic_int_t *terminal_alive, *terminal_ack;
static int terminal_value, terminal_armed, terminal_erased;

static int terminal_native_load(retro_atomic_int_t *value)
{
   return retro_atomic_load_acquire_int(value);
}

static void terminal_native_store(retro_atomic_int_t *target, int value)
{
   retro_atomic_store_release_int(target, value);
}

static int terminal_load(retro_atomic_int_t *target)
{
   int value = terminal_native_load(target);
   if (target == terminal_alive && terminal_armed && value)
   {
      terminal_armed = 0;
      terminal_native_store(terminal_alive, 0);
      terminal_native_store(terminal_ack, terminal_value);
   }
   return value;
}

static void terminal_store(retro_atomic_int_t *target, int value)
{
   if (target == terminal_ack && !value && terminal_native_load(target) == terminal_value)
   {
      terminal_erased++;
      return;
   }
   terminal_native_store(target, value);
}

#undef retro_atomic_load_acquire_int
#undef retro_atomic_store_release_int
#define retro_atomic_load_acquire_int terminal_load
#define retro_atomic_store_release_int terminal_store
#define audio_thread_parked terminal_parked
#define audio_thread_wrapped_driver terminal_wrapped_driver
#define audio_thread_wrapped_ident terminal_wrapped_ident
#define audio_init_thread terminal_init_thread
#include "../../../audio/audio_thread_wrapper.c"
#undef retro_atomic_load_acquire_int
#undef retro_atomic_store_release_int
#undef audio_thread_parked
#undef audio_thread_wrapped_driver
#undef audio_thread_wrapped_ident
#undef audio_init_thread

static unsigned terminal_freed;

static void *terminal_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   static int handle;
   (void)device; (void)rate; (void)latency; (void)new_rate;
   return &handle;
}

static bool terminal_start(void *data, bool shutdown)
{
   (void)data; (void)shutdown;
   return true;
}

static void terminal_free(void *data)
{
   (void)data;
   terminal_freed++;
}

void audio_thread_terminal_ack_test(void)
{
   audio_thread_t thread;
   audio_driver_t driver;
   memset(&thread, 0, sizeof(thread));
   memset(&driver, 0, sizeof(driver));
   driver.init = terminal_init;
   driver.start = terminal_start;
   driver.free = terminal_free;
   driver.ident = "terminal";
   thread.driver = &driver;
   retro_eventcount_init(&thread.ec);
   audio_thread_loop(&thread);
   assert(terminal_freed == 1);
   assert(terminal_native_load(&thread.stopped_ack) == AUDIO_THREAD_ACK_EXITED);
   terminal_alive = &thread.alive;
   terminal_ack = &thread.stopped_ack;
   terminal_value = AUDIO_THREAD_ACK_EXITED;
   terminal_native_store(terminal_alive, 1);
   terminal_native_store(&thread.stopped, 0);
   terminal_native_store(terminal_ack, 1);
   terminal_armed = 1;
   audio_thread_block(&thread);
   assert(!terminal_armed && !terminal_erased);
   assert(terminal_native_load(terminal_ack) == AUDIO_THREAD_ACK_EXITED);
   assert(terminal_native_load(&thread.stopped));
   terminal_alive = terminal_ack = NULL;
   retro_eventcount_free(&thread.ec);
   puts("terminal acknowledgement: late stop cannot erase worker exit");
}

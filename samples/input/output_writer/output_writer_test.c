/* Shared output writer: timed service, idle sleep and final ownership. */
#include <stdio.h>
#include <string.h>
#include <rthreads/rthreads.h>
#include "input/common/output_writer.h"

#if !defined(_WIN32) && defined(HAVE_THREADS)
#include <errno.h>
#include <unistd.h>
#include <retro_atomic.h>
static retro_atomic_int_t inject_eintr;
static ssize_t test_writer_write(int fd, const void *buf, size_t size)
{
   if (retro_atomic_exchange_int(&inject_eintr, 0))
   {
      errno = EINTR;
      return -1;
   }
   return write(fd, buf, size);
}
#define write(fd, buf, size) test_writer_write((fd), (buf), (size))
#endif
#include "input/common/output_writer.c"
#if !defined(_WIN32) && defined(HAVE_THREADS)
#undef write
#endif

struct test_state
{
   slock_t *lock;
   scond_t *cond;
   unsigned calls;
   unsigned finals;
   unsigned serviced; /* writer-owned */
   int timeout_ms;
};

static void service(void *data, bool last)
{
   struct test_state *state = (struct test_state*)data;
   if (!last)
      state->serviced++;
   slock_lock(state->lock);
   if (last)
      state->finals++;
   else
      state->calls++;
   scond_signal(state->cond);
   slock_unlock(state->lock);
}

static int next_timeout(void *data)
{
   struct test_state *state = (struct test_state*)data;
   return state->serviced == 1 ? state->timeout_ms : -1;
}

static bool await_calls(struct test_state *state, unsigned want)
{
   unsigned tries = 0;
   bool ok;
   slock_lock(state->lock);
   while (state->calls < want && tries++ < 5)
      scond_wait_timeout(state->cond, state->lock, 200000);
   ok = state->calls == want;
   slock_unlock(state->lock);
   return ok;
}

static bool remains_idle(struct test_state *state, unsigned want)
{
   bool ok;
   slock_lock(state->lock);
   scond_wait_timeout(state->cond, state->lock, 40000);
   ok = state->calls == want;
   slock_unlock(state->lock);
   return ok;
}

int main(void)
{
   struct test_state state;
   input_output_writer_t *writer;
   bool ok;
   memset(&state, 0, sizeof(state));
   state.timeout_ms = 20;
   state.lock = slock_new();
   state.cond = scond_new();
   if (!state.lock || !state.cond)
      return 2;
   writer = input_output_writer_new_timed(service, next_timeout, &state);
#ifndef HAVE_THREADS
   ok = writer == NULL;
#else
   if (!writer)
      return 2;
   ok = remains_idle(&state, 0);
#if !defined(_WIN32)
   retro_atomic_store_release_int(&inject_eintr, 1);
#endif
   input_output_writer_wake(writer);
   ok = await_calls(&state, 2) && ok; /* one wake, one timeout */
   ok = remains_idle(&state, 2) && ok;
   input_output_writer_wake(writer);
   ok = await_calls(&state, 3) && ok;
   input_output_writer_free(writer);
   ok = state.finals == 1 && ok;
   state.calls = 0;
   state.finals = state.serviced = 0;
   state.timeout_ms = 5000;
   writer = input_output_writer_new_timed(service, next_timeout, &state);
   if (!writer)
      return 2;
   input_output_writer_wake(writer);
   ok = await_calls(&state, 1) && ok;
   input_output_writer_free(writer); /* interrupts a pending timed wait */
   ok = state.finals == 1 && ok;
   state.calls = state.finals = state.serviced = 0;
   writer = input_output_writer_new(service, &state);
   if (!writer)
      return 2;
   input_output_writer_wake(writer);
   ok = await_calls(&state, 1) && ok;
   ok = remains_idle(&state, 1) && ok;
   input_output_writer_free(writer);
   ok = state.finals == 1 && ok;
#endif
   scond_free(state.cond);
   slock_free(state.lock);
   puts(ok ? "PASS output_writer_test" : "FAIL output_writer_test");
   return ok ? 0 : 1;
}

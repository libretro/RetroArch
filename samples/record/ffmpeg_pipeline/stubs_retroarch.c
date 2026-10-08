/* What record_ffmpeg.c reaches for in RetroArch, stubbed. */
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <boolean.h>
#include <queues/message_queue.h>
#include "../../../runloop.h"

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

/* What the frontend was told, counted: drop mode must say nothing
 * while it records. */
unsigned stub_warn_calls = 0;
unsigned stub_msg_calls  = 0;

void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   stub_warn_calls++;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush,
      char *title, enum message_queue_icon icon,
      enum message_queue_category category)
{
   stub_msg_calls++;
}

void RARCH_DBG(const char *fmt, ...)
{
}

bool verbosity_is_enabled(void)
{
   return false;
}

unsigned verbosity_get_log_level(void)
{
   return 1;
}

static runloop_state_t stub_runloop_state;

runloop_state_t *runloop_state_get_ptr(void)
{
   return &stub_runloop_state;
}

void rarch_perf_register(struct retro_perf_counter *perf)
{
   perf->registered = true;
}

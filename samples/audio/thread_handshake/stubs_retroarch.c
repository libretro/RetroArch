/* Frontend symbols audio/audio_thread_wrapper.c references. The
 * warning counter is what the handshake cases assert on: RARCH_WARN
 * is the wrapper's only report that a handshake has stalled. */

#include <stdio.h>
#include <stdarg.h>

#include <boolean.h>
#include <retro_atomic.h>

retro_atomic_int_t warn_count = RETRO_ATOMIC_INT_INITIALIZER(0);

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   retro_atomic_fetch_add_int(&warn_count, 1);
   va_start(ap, fmt);
   printf("WARN: ");
   vprintf(fmt, ap);
   va_end(ap);
}

/* The wrapper drives these around its stop/start transitions; the
 * pipeline is not under test here. */
void audio_driver_disable_callback(void) { }
void audio_driver_enable_callback(void) { }
void audio_driver_pipeline_wake(void) { }
void audio_driver_pipeline_consumer_exit(void) { }

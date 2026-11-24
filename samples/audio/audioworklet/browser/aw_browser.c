/* audio/drivers/audioworklet.c in a real browser, by hand: run.sh
 * builds it for each of the driver's build shapes and runs it in
 * headless Chrome.  Each run brings the driver up, plays three seconds
 * of tone through the worklet with blocking writes - the writer parked
 * on the worklet's pulls, never sleeping - then takes the
 * driver down and up again while the worklet pulls.  The init is to be
 * woken by the worklet's report, the writes paced by its pulls with
 * nothing dropped, and every retired ring freed.  Lines go to the
 * page's log; the last is RESULT PASS or RESULT FAIL. */

#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <emscripten.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>

#include "audio/drivers/audioworklet.c"

void RARCH_LOG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("ERR: "); vprintf(fmt, ap); va_end(ap); }

void platform_emscripten_run_on_browser_thread_sync(void (*func)(void*), void *arg)
{
#ifdef PROXY_TO_PTHREAD
   emscripten_proxy_sync(emscripten_proxy_get_system_queue(),
         emscripten_main_runtime_thread_id(), func, arg);
#else
   func(arg);
#endif
}
void platform_emscripten_enter_fake_block(int ms) { (void)ms; }
void platform_emscripten_exit_fake_block(void) { }
void retro_sleep(unsigned ms)
{
#ifdef PROXY_TO_PTHREAD
   emscripten_thread_sleep(ms);
#else
   emscripten_sleep(ms);
#endif
}

#define TONE_FRAMES 800

static float  tone_buf[2 * TONE_FRAMES];
static double tone_phase;

static void tone(unsigned rate)
{
   unsigned i;
   for (i = 0; i < TONE_FRAMES; i++)
   {
      float v     = 0.2f * (float)sin(tone_phase);
      tone_phase += 2.0 * 3.14159265 * 440.0 / rate;
      tone_buf[i * 2] = tone_buf[i * 2 + 1] = v;
   }
}

int main(void)
{
   unsigned rate = 0, cycle, frames_out = 0, dropped;
   /* The harness's own clock, for its report only. */
   double t0 = emscripten_get_now(), t1;
   audioworklet_data_t *aw = (audioworklet_data_t*)
      audioworklet_init(NULL, 48000, 64, &rate);
   t1 = emscripten_get_now();
   printf("init %s in %.0f ms, rate %u\n", aw ? "ok" : "FAILED", t1 - t0, rate);
   if (!aw)
   {
      printf("RESULT FAIL\n");
      return 0;
   }
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
   while (!aw->driver_running)
   {
      audioworklet_external_block();
      emscripten_sleep(1);
   }
   printf("external-block init done after %.0f ms\n", emscripten_get_now() - t0);
#endif
   for (cycle = 0; cycle < 200 && !audioworklet_resume_ctx(aw); cycle++)
      retro_sleep(10);
   printf("context running: %d\n", (int)aw->context_running);

   t0 = emscripten_get_now();
   while (emscripten_get_now() - t0 < 3000)
   {
      ssize_t w;
      tone(rate);
      w = audioworklet_write(aw, tone_buf, sizeof(tone_buf));
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
      audioworklet_external_block();
#endif
      frames_out += (unsigned)(w / (2 * sizeof(float)));
   }
   t1      = emscripten_get_now();
   dropped = (unsigned)retro_atomic_load_acquire_size(&aw->dropped);
   printf("wrote %u frames in %.0f ms (%.0f frames/s), underruns %u, dropped %u\n",
         frames_out, t1 - t0, frames_out * 1000.0 / (t1 - t0),
         (unsigned)audioworklet_underruns(aw), dropped);

   for (cycle = 0; cycle < 200; cycle++)
   {
      audioworklet_free(aw);
      if (audioworklet_init(NULL, 48000, 32 + (cycle & 31), &rate) != aw)
      {
         printf("reinit FAILED at %u\nRESULT FAIL\n", cycle);
         return 0;
      }
      tone(rate);
      audioworklet_resume_ctx(aw);
      audioworklet_write(aw, tone_buf, sizeof(tone_buf));
      if (!(cycle & 15))
         retro_sleep(3);
   }
   audioworklet_free(aw);
   /* One pull at most can hold a ring back; the next drain frees it. */
   retro_sleep(10);
   audioworklet_retired_drain(aw);
   printf("cycles done, retired rings left %d\n", aw->retired != NULL);
   printf("RESULT %s\n",
            frames_out > rate * 2 && !dropped && !aw->retired
         ? "PASS" : "FAIL");
   return 0;
}

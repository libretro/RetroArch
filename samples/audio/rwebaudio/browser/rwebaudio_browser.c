/* audio/drivers/rwebaudio.c and its JavaScript half in a real browser,
 * by hand: run.sh builds it for each asyncify build shape and runs it
 * in headless Chrome.  Each run plays three seconds of tone with
 * blocking writes - the page's thread awaiting its context's clock for
 * room, never sleeping - and is to keep up with the device rate.
 * Lines go to the page's log; the last is RESULT PASS or RESULT FAIL. */

#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <emscripten.h>

#include "audio/drivers/rwebaudio.c"

void RARCH_LOG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("ERR: "); vprintf(fmt, ap); va_end(ap); }

uint32_t audio_driver_requested_layout(void) { return AUDIO_LAYOUT_STEREO; }
enum platform_emscripten_browser platform_emscripten_get_browser(void)
{ return PLATFORM_EMSCRIPTEN_BROWSER_CHROMIUM; }
void platform_emscripten_enter_fake_block(int ms) { (void)ms; }
void platform_emscripten_exit_fake_block(void) { }

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
   unsigned rate = 0, i, frames_out = 0;
   /* The harness's own clock, for its report only. */
   double t0, t1;
   void *h = audio_rwebaudio.init(NULL, 48000, 64, &rate);
   printf("init %s, rate %u\n", h ? "ok" : "FAILED", rate);
   if (!h)
   {
      printf("RESULT FAIL\n");
      return 0;
   }
   audio_rwebaudio.set_nonblock_state(h, false);
   /* The autoplay flag lets the context run; give it the chance. */
   for (i = 0; i < 100 && !RWebAudioResumeCtx(); i++)
      emscripten_sleep(10);
   printf("context running: %d\n", (int)RWebAudioResumeCtx());

   t0 = emscripten_get_now();
   while (emscripten_get_now() - t0 < 3000)
   {
      ssize_t w;
      tone(rate);
      w = audio_rwebaudio.write(h, tone_buf, sizeof(tone_buf));
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
      rwebaudio_external_block();
#endif
      frames_out += (unsigned)(w / (2 * sizeof(float)));
   }
   t1 = emscripten_get_now();
   printf("wrote %u frames in %.0f ms (%.0f frames/s)\n",
         frames_out, t1 - t0, frames_out * 1000.0 / (t1 - t0));
   audio_rwebaudio.free(h);
   /* Paced by the context: as many frames as it plays, give or take
    * the latency queued at the start. */
   printf("RESULT %s\n",
            frames_out > rate * 2.8 && frames_out < rate * 3.3
         ? "PASS" : "FAIL");
   return 0;
}

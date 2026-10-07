/* Audio and controller checks, run in Dolphin by
 * os/gekko/tests/run-dolphin.sh: buffers start at the rate their
 * length and the sample rate say, and port 1 has a controller at
 * rest while the others are empty. */

#include <stdio.h>
#include <string.h>

#include <gekko/audio.h>
#include <gekko/pad.h>
#include <gekko/power.h>
#include <gekko/thread.h>

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (cond) \
         gk_debug_printf("ok   %s", what); \
      else \
      { \
         gk_debug_printf("FAIL %s", what); \
         failures++; \
      } \
   } while (0)

#define BUF_BYTES 3200   /* 800 stereo frames */

static int16_t buf[2][BUF_BYTES / 2] __attribute__((aligned(32)));
static volatile uint32_t started;
static unsigned next_buf;

static void on_buffer(void *data)
{
   (void)data;
   started++;
   gk_audio_queue(buf[next_buf], BUF_BYTES);
   next_buf ^= 1;
}

static void test_audio(unsigned rate)
{
   char what[96];
   uint64_t t0;
   uint32_t n0;
   double per_s, want = (double)rate * 4 / BUF_BYTES;
   gk_audio_init(rate);
   gk_audio_set_cb(on_buffer, NULL);
   next_buf = 1;
   gk_audio_queue(buf[0], BUF_BYTES);
   gk_audio_start();
   gk_sleep_us(100000);
   n0 = started;
   t0 = gk_ticks();
   gk_sleep_us(1000000);
   per_s = (started - n0) * (double)gk_tb_hz / (double)(gk_ticks() - t0);
   gk_audio_stop();
   gk_audio_set_cb(NULL, NULL);
   snprintf(what, sizeof(what), "audio %u Hz: %.1f buffers/s, want %.1f",
         rate, per_s, want);
   CHECK(per_s > want * 0.97 && per_s < want * 1.03, what);
}

static void test_pads(void)
{
   gk_pad_t p[GK_PAD_PORTS];
   uint32_t polls;
   gk_pad_init();
   gk_sleep_us(100000);
   gk_pad_read(p);
   polls = p[0].polls;
   gk_sleep_us(100000);
   gk_pad_read(p);
   gk_debug_printf("     %u reports in 100 ms", (unsigned)(p[0].polls - polls));
   /* Hardware polls once a field (Y = 1); Dolphin repeats every X
    * lines, so only the lower bound is the hardware's. */
   CHECK(p[0].polls - polls >= 5, "port 1 reports at least once a field");
   gk_debug_printf("     ports: %d %d %d %d, port 1 buttons %04x stick %d,%d",
         p[0].connected, p[1].connected, p[2].connected, p[3].connected,
         p[0].buttons, p[0].stick_x, p[0].stick_y);
   CHECK(p[0].connected && !p[1].connected && !p[2].connected
         && !p[3].connected, "controller found on port 1 only");
   CHECK(p[0].buttons == 0 && p[0].stick_x > -8 && p[0].stick_x < 8
         && p[0].stick_y > -8 && p[0].stick_y < 8,
         "port 1 at rest: no buttons, sticks at the origin");
}

int main(int argc, char **argv)
{
   unsigned i;
   (void)argc;
   (void)argv;
   for (i = 0; i < BUF_BYTES / 2; i++)
      buf[0][i] = buf[1][i] = (int16_t)((i & 64) ? 2000 : -2000);
   gk_dcache_flush(buf, sizeof(buf));
   test_audio(48000);
   test_audio(32000);
   test_pads();
   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
#ifdef HW_RVL
   gk_power_off();
#endif
   return 0;
}
